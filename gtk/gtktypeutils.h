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

#ifndef __GTK_TYPE_UTILS_H__
#define __GTK_TYPE_UTILS_H__


#include <glib.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Fundamental Types
 */
typedef enum
{
  GTK_TYPE_INVALID,
  GTK_TYPE_NONE,
  
  /* flat types */
  GTK_TYPE_CHAR,
  GTK_TYPE_UCHAR,
  GTK_TYPE_BOOL,
  GTK_TYPE_INT,
  GTK_TYPE_UINT,
  GTK_TYPE_LONG,
  GTK_TYPE_ULONG,
  GTK_TYPE_FLOAT,
  GTK_TYPE_DOUBLE,
  GTK_TYPE_STRING,
  GTK_TYPE_ENUM,
  GTK_TYPE_FLAGS,
  GTK_TYPE_BOXED,
  GTK_TYPE_POINTER,
  
  /* structured types */
  GTK_TYPE_SIGNAL,
  GTK_TYPE_ARGS,
  GTK_TYPE_CALLBACK,
  GTK_TYPE_C_CALLBACK,
  GTK_TYPE_FOREIGN,
  
  /* base type node of the object system */
  GTK_TYPE_OBJECT
} GtkFundamentalType;

/* bounds definitions for type sets, these are provided to distinct
 * between fundamental types with if() statements, and to build
 * up foreign fundamentals
 */
#define	GTK_TYPE_FLAT_FIRST		GTK_TYPE_CHAR
#define	GTK_TYPE_FLAT_LAST		GTK_TYPE_POINTER
#define	GTK_TYPE_STRUCTURED_FIRST	GTK_TYPE_SIGNAL
#define	GTK_TYPE_STRUCTURED_LAST	GTK_TYPE_FOREIGN
#define	GTK_TYPE_FUNDAMENTAL_LAST	GTK_TYPE_OBJECT
#define	GTK_TYPE_FUNDAMENTAL_MAX	(32)


/* retrive a structure offset */
#ifdef offsetof
#define GTK_STRUCT_OFFSET(struct, field)        ((gint) offsetof (struct, field))
#else /* !offsetof */
#define GTK_STRUCT_OFFSET(struct, field)        ((gint) ((gchar*) &((struct*) 0)->field))
#endif /* !offsetof */


/* The debugging versions of the casting macros make sure the cast is "ok"
 *  before proceeding, but they are definately slower than their less
 *  careful counterparts as they involve extra ``is a'' checks.
 */
#ifdef GTK_NO_CHECK_CASTS
#  define GTK_CHECK_CAST(tobj, cast_type, cast)       ((cast*) (tobj))
#  define GTK_CHECK_CLASS_CAST(tclass,cast_type,cast) ((cast*) (tclass))
#else /* !GTK_NO_CHECK_CASTS */
#  define GTK_CHECK_CAST(tobj, cast_type, cast) \
      ((cast*) gtk_type_check_object_cast ((GtkTypeObject*) (tobj), (cast_type)))
#  define GTK_CHECK_CLASS_CAST(tclass,cast_type,cast) \
      ((cast*) gtk_type_check_class_cast ((GtkTypeClass*) (tclass), (cast_type)))
#endif /* GTK_NO_CHECK_CASTS */

/* Determines whether `type_object' and `type_class' are a type of `otype'.
 */
#define GTK_CHECK_TYPE(type_object, otype)       ( \
  ((GtkTypeObject*) (type_object)) != NULL && \
  GTK_CHECK_CLASS_TYPE (((GtkTypeObject*) (type_object))->klass, (otype)) \
)
#define GTK_CHECK_CLASS_TYPE(type_class, otype)  ( \
  ((GtkTypeClass*) (type_class)) != NULL && \
  gtk_type_is_a (((GtkTypeClass*) (type_class))->type, (otype)) \
)




/* A GtkType holds a unique type id
 */
typedef guint GtkType;

typedef struct _GtkTypeObject	GtkTypeObject;
typedef struct _GtkTypeClass	GtkTypeClass;


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* Builtin Types
 */
#include <gtk/gtktypebuiltins.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define		GTK_TYPE_IDENTIFIER		(gtk_identifier_get_type ())
GtkType		gtk_identifier_get_type		(void);

/* Macros
 */
#define GTK_TYPE_MAKE(parent_t, seqno)	(((seqno) << 8) | GTK_FUNDAMENTAL_TYPE (parent_t))
#define GTK_FUNDAMENTAL_TYPE(type)	((GtkFundamentalType) ((type) & 0xFF))
#define GTK_TYPE_SEQNO(type)		((type) > 0xFF ? (type) >> 8 : (type))

typedef struct _GtkArg	       GtkArg;
typedef struct _GtkObject      GtkObject;   /* forward declaration of object type */
typedef struct _GtkTypeInfo    GtkTypeInfo;
typedef struct _GtkTypeQuery   GtkTypeQuery;
typedef struct _GtkEnumValue   GtkEnumValue;
typedef struct _GtkEnumValue   GtkFlagValue;


#define GTK_SIGNAL_FUNC(f)  ((GtkSignalFunc) f)

typedef void (*GtkClassInitFunc)   (gpointer   klass);
typedef void (*GtkObjectInitFunc)  (gpointer   object,
				    gpointer   klass);
typedef void (*GtkSignalFunc)      ();
typedef gint (*GtkFunction)	   (gpointer   data);
typedef void (*GtkDestroyNotify)   (gpointer   data);
typedef void (*GtkCallbackMarshal) (GtkObject *object,
				    gpointer   data,
				    guint      n_args,
				    GtkArg    *args);
typedef void (*GtkSignalMarshaller) (GtkObject      *object,
				     GtkSignalFunc   func,
				     gpointer        func_data,
				     GtkArg         *args);

/* deprecated */
typedef void (*GtkArgGetFunc)	   (GtkObject*, GtkArg*, guint);
typedef void (*GtkArgSetFunc)	   (GtkObject*, GtkArg*, guint);


/* A GtkTypeObject defines the minimum structure requirements
 * for type instances. Type instances returned from gtk_type_new ()
 * and initialized through a GtkObjectInitFunc need to directly inherit
 * from this structure or at least copy its fields one by one.
 */
struct _GtkTypeObject
{
  /* A pointer to the objects class. This will actually point to
   *  the derived objects class struct (which will be derived from
   *  GtkTypeClass).
   */
  GtkTypeClass	*klass;
};


/* A GtkTypeClass defines the minimum structure requirements for
 * a types class. Classes returned from gtk_type_class () and
 * initialized through a GtkClassInitFunc need to directly inherit
 * from this structure or at least copy its fields one by one.
 */
struct _GtkTypeClass
{
  /* The type identifier for the objects class. There is
   *  one unique identifier per class.
   */
  GtkType type;
};


struct _GtkArg
{
  GtkType type;
  gchar *name;
  
  /* this union only defines the required storage types for
   * the possibile values, thus there is no gint enum_data field,
   * because that would just be a mere alias for gint int_data.
   * use the GTK_VALUE_*() and GTK_RETLOC_*() macros to access
   * the discrete memebers.
   */
  union {
    /* flat values */
    gchar char_data;
    guchar uchar_data;
    gboolean bool_data;
    gint int_data;
    guint uint_data;
    glong long_data;
    gulong ulong_data;
    gfloat float_data;
    gdouble double_data;
    gchar *string_data;
    gpointer pointer_data;
    GtkObject *object_data;
    
    /* structured values */
    struct {
      GtkSignalFunc f;
      gpointer d;
    } signal_data;
    struct {
      gint n_args;
      GtkArg *args;
    } args_data;
    struct {
      GtkCallbackMarshal marshal;
      gpointer data;
      GtkDestroyNotify notify;
    } callback_data;
    struct {
      GtkFunction func;
      gpointer func_data;
    } c_callback_data;
    struct {
      gpointer data;
      GtkDestroyNotify notify;
    } foreign_data;
  } d;
};

/* argument value access macros, these must not contain casts,
 * to allow the usage of these macros in combination with the
 * adress operator, e.g. &GTK_VALUE_CHAR (*arg)
 */

/* flat values */
#define GTK_VALUE_CHAR(a)	((a).d.char_data)
#define GTK_VALUE_UCHAR(a)	((a).d.uchar_data)
#define GTK_VALUE_BOOL(a)	((a).d.bool_data)
#define GTK_VALUE_INT(a)	((a).d.int_data)
#define GTK_VALUE_UINT(a)	((a).d.uint_data)
#define GTK_VALUE_LONG(a)	((a).d.long_data)
#define GTK_VALUE_ULONG(a)	((a).d.ulong_data)
#define GTK_VALUE_FLOAT(a)	((a).d.float_data)
#define GTK_VALUE_DOUBLE(a)	((a).d.double_data)
#define GTK_VALUE_STRING(a)	((a).d.string_data)
#define GTK_VALUE_ENUM(a)	((a).d.int_data)
#define GTK_VALUE_FLAGS(a)	((a).d.uint_data)
#define GTK_VALUE_BOXED(a)	((a).d.pointer_data)
#define GTK_VALUE_POINTER(a)	((a).d.pointer_data)
#define GTK_VALUE_OBJECT(a)	((a).d.object_data)

/* structured values */
#define GTK_VALUE_SIGNAL(a)	((a).d.signal_data)
#define GTK_VALUE_ARGS(a)	((a).d.args_data)
#define GTK_VALUE_CALLBACK(a)	((a).d.callback_data)
#define GTK_VALUE_C_CALLBACK(a) ((a).d.c_callback_data)
#define GTK_VALUE_FOREIGN(a)	((a).d.foreign_data)

/* return location macros, these all narow down to
 * pointer types, because return values need to be
 * passed by reference
 */

/* flat values */
#define GTK_RETLOC_CHAR(a)	((gchar*)	(a).d.pointer_data)
#define GTK_RETLOC_UCHAR(a)	((guchar*)	(a).d.pointer_data)
#define GTK_RETLOC_BOOL(a)	((gboolean*)	(a).d.pointer_data)
#define GTK_RETLOC_INT(a)	((gint*)	(a).d.pointer_data)
#define GTK_RETLOC_UINT(a)	((guint*)	(a).d.pointer_data)
#define GTK_RETLOC_LONG(a)	((glong*)	(a).d.pointer_data)
#define GTK_RETLOC_ULONG(a)	((gulong*)	(a).d.pointer_data)
#define GTK_RETLOC_FLOAT(a)	((gfloat*)	(a).d.pointer_data)
#define GTK_RETLOC_DOUBLE(a)	((gdouble*)	(a).d.pointer_data)
#define GTK_RETLOC_STRING(a)	((gchar**)	(a).d.pointer_data)
#define GTK_RETLOC_ENUM(a)	((gint*)	(a).d.pointer_data)
#define GTK_RETLOC_FLAGS(a)	((guint*)	(a).d.pointer_data)
#define GTK_RETLOC_BOXED(a)	((gpointer*)	(a).d.pointer_data)
#define GTK_RETLOC_POINTER(a)	((gpointer*)	(a).d.pointer_data)
#define GTK_RETLOC_OBJECT(a)	((GtkObject**)	(a).d.pointer_data)

struct _GtkTypeInfo
{
  gchar			*type_name;
  guint			 object_size;
  guint			 class_size;
  GtkClassInitFunc	 class_init_func;
  GtkObjectInitFunc	 object_init_func;
  gpointer		 reserved_1;
  gpointer		 reserved_2;
  GtkClassInitFunc	 base_class_init_func;
};

struct _GtkTypeQuery
{
  GtkType		 type;
  const gchar		*type_name;
  guint			 object_size;
  guint			 class_size;
};

struct _GtkEnumValue
{
  guint	 value;
  gchar	*value_name;
  gchar *value_nick;
};


void		gtk_type_init			(void);
GtkType		gtk_type_unique			(GtkType	   parent_type,
						 const GtkTypeInfo *type_info);
void		gtk_type_set_chunk_alloc	(GtkType	 type,
						 guint		 n_chunks);
gchar*		gtk_type_name			(guint		 type);
GtkType		gtk_type_from_name		(const gchar	*name);
GtkType		gtk_type_parent			(GtkType	 type);
gpointer	gtk_type_class			(GtkType	 type);
gpointer	gtk_type_parent_class		(GtkType	 type);
GList*		gtk_type_children_types		(GtkType	 type);
gpointer	gtk_type_new			(GtkType	 type);
void		gtk_type_free			(GtkType	 type,
						 gpointer	 mem);
void		gtk_type_describe_heritage	(GtkType	 type);
void		gtk_type_describe_tree		(GtkType	 type,
						 gboolean	 show_size);
gboolean	gtk_type_is_a			(GtkType	 type,
						 GtkType	 is_a_type);
GtkTypeObject*	gtk_type_check_object_cast	(GtkTypeObject	*type_object,
						 GtkType         cast_type);
GtkTypeClass*	gtk_type_check_class_cast	(GtkTypeClass	*klass,
						 GtkType         cast_type);
GtkType		gtk_type_register_enum		(const gchar	*type_name,
						 GtkEnumValue	*values);
GtkType		gtk_type_register_flags		(const gchar	*type_name,
						 GtkFlagValue	*values);
GtkEnumValue*	gtk_type_enum_get_values	(GtkType	 enum_type);
GtkFlagValue*	gtk_type_flags_get_values	(GtkType	 flags_type);
GtkEnumValue*	gtk_type_enum_find_value	(GtkType	 enum_type,
						 const gchar	*value_name);
GtkFlagValue*	gtk_type_flags_find_value	(GtkType	 flag_type,
						 const gchar	*value_name);
/* set the argument collector alias for foreign fundamentals */
void		gtk_type_set_varargs_type	(GtkType	foreign_type,
						 GtkType	varargs_type);
GtkType		gtk_type_get_varargs_type	(GtkType	foreign_type);
/* Report internal information about a type. The caller has the
 * responsibility to invoke a subsequent g_free (returned_data); but
 * must not modify data pointed to by the members of GtkTypeQuery
 */
GtkTypeQuery*	gtk_type_query			(GtkType	type);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TYPE_UTILS_H__ */
