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
#ifndef __GTK_TYPE_UTILS_H__
#define __GTK_TYPE_UTILS_H__


#include <gdk/gdk.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


/* Fundamental Types
 */
typedef enum
{
  GTK_TYPE_INVALID,
  GTK_TYPE_NONE,
  GTK_TYPE_CHAR,
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
  GTK_TYPE_FOREIGN,
  GTK_TYPE_CALLBACK,
  GTK_TYPE_ARGS,
  
  GTK_TYPE_POINTER,
  
  /* It'd be great if the next two could be removed eventually
   */
  GTK_TYPE_SIGNAL,
  GTK_TYPE_C_CALLBACK,
  
  GTK_TYPE_OBJECT
  
} GtkFundamentalType;

typedef guint GtkType;

/* Builtin Types
 */
extern GtkType gtk_type_builtins[];
#include <gtk/gtktypebuiltins.h>

/* Macros
 */
#define GTK_TYPE_MAKE(parent_t, seqno) 	(((seqno) << 8) | GTK_FUNDAMENTAL_TYPE (parent_t))
#define GTK_FUNDAMENTAL_TYPE(type)	 	((GtkFundamentalType) ((type) & 0xFF))
#define GTK_TYPE_SEQNO(type)	 	((type) > 0xFF ? (type) >> 8 : (type))

typedef struct _GtkArg	       GtkArg;
typedef struct _GtkObject      GtkObject;   /* forward declaration of object type */
typedef struct _GtkTypeInfo    GtkTypeInfo;

typedef void (*GtkClassInitFunc)  (gpointer klass);
typedef void (*GtkObjectInitFunc) (gpointer object);
typedef void (*GtkArgGetFunc) (GtkObject *object, GtkArg *arg, guint arg_id);
typedef void (*GtkArgSetFunc) (GtkObject *object, GtkArg *arg, guint arg_id);
typedef gint (*GtkFunction) (gpointer data);
typedef void (*GtkCallbackMarshal) (GtkObject *object,
				    gpointer data,
				    guint n_args,
				    GtkArg *args);
typedef void (*GtkDestroyNotify) (gpointer data);

struct _GtkArg
{
  GtkType type;
  gchar *name;

  union {
    gchar char_data;
    gint int_data;
    guint uint_data;
    gint bool_data;
    glong long_data;
    gulong ulong_data;
    gfloat float_data;
    gdouble double_data;
    gchar *string_data;
    gpointer pointer_data;
    GtkObject *object_data;
    struct {
      GtkCallbackMarshal marshal;
      gpointer data;
      GtkDestroyNotify notify;
    } callback_data;
    struct {
      gpointer data;
      GtkDestroyNotify notify;
    } foreign_data;
    struct {
      gint n_args;
      GtkArg *args;
    } args_data;
    struct {
      GtkFunction f;
      gpointer d;
    } signal_data;
    struct {
      GtkFunction func;
      gpointer func_data;
    } c_callback_data;
  } d;
};

#define GTK_VALUE_CHAR(a)	((a).d.char_data)
#define GTK_VALUE_BOOL(a)	((a).d.bool_data)
#define GTK_VALUE_INT(a)	((a).d.int_data)
#define GTK_VALUE_UINT(a)	((a).d.uint_data)
#define GTK_VALUE_LONG(a)	((a).d.long_data)
#define GTK_VALUE_ULONG(a)	((a).d.ulong_data)
#define GTK_VALUE_FLOAT(a)	((a).d.float_data)
#define GTK_VALUE_DOUBLE(a)	((a).d.double_data)
#define GTK_VALUE_STRING(a)	((a).d.string_data)
#define GTK_VALUE_ENUM(a)	((a).d.int_data)
#define GTK_VALUE_FLAGS(a)	((a).d.int_data)
#define GTK_VALUE_BOXED(a)	((a).d.pointer_data)
#define GTK_VALUE_FOREIGN(a)	((a).d.foreign_data)
#define GTK_VALUE_CALLBACK(a)	((a).d.callback_data)
#define GTK_VALUE_ARGS(a)	((a).d.args_data)
#define GTK_VALUE_OBJECT(a)	((a).d.object_data)
#define GTK_VALUE_POINTER(a)	((a).d.pointer_data)
#define GTK_VALUE_SIGNAL(a)	((a).d.signal_data)
#define GTK_VALUE_C_CALLBACK(a) ((a).d.c_callback_data)

#define GTK_RETLOC_CHAR(a)	((gchar*)(a).d.pointer_data)
#define GTK_RETLOC_BOOL(a)	((gint*)(a).d.pointer_data)
#define GTK_RETLOC_INT(a)	((gint*)(a).d.pointer_data)
#define GTK_RETLOC_UINT(a)	((guint*)(a).d.pointer_data)
#define GTK_RETLOC_LONG(a)	((glong*)(a).d.pointer_data)
#define GTK_RETLOC_ULONG(a)	((gulong*)(a).d.pointer_data)
#define GTK_RETLOC_FLOAT(a)	((gfloat*)(a).d.pointer_data)
#define GTK_RETLOC_DOUBLE(a)	((gdouble*)(a).d.pointer_data)
#define GTK_RETLOC_STRING(a)	((gchar**)(a).d.pointer_data)
#define GTK_RETLOC_ENUM(a)	((gint*)(a).d.pointer_data)
#define GTK_RETLOC_FLAGS(a)	((gint*)(a).d.pointer_data)
#define GTK_RETLOC_BOXED(a)	((gpointer*)(a).d.pointer_data)
#define GTK_RETLOC_OBJECT(a)	((GtkObject**)(a).d.pointer_data)
#define GTK_RETLOC_POINTER(a)	((gpointer*)(a).d.pointer_data)

struct _GtkTypeInfo
{
  gchar *type_name;
  guint object_size;
  guint class_size;
  GtkClassInitFunc class_init_func;
  GtkObjectInitFunc object_init_func;
  GtkArgSetFunc arg_set_func;
  GtkArgGetFunc arg_get_func;
};


void	 gtk_type_init		    (void);
GtkType	 gtk_type_unique	    (guint	  parent_type,
				     GtkTypeInfo *type_info);
gchar*	 gtk_type_name		    (guint	  type);
GtkType	 gtk_type_from_name	    (const gchar *name);
GtkType	 gtk_type_parent	    (GtkType	  type);
gpointer gtk_type_class		    (GtkType	  type);
gpointer gtk_type_parent_class	    (GtkType	  type);
gpointer gtk_type_new		    (GtkType	  type);
void	 gtk_type_describe_heritage (GtkType	  type);
void	 gtk_type_describe_tree	    (GtkType	  type,
				     gint	  show_size);
gint	 gtk_type_is_a		    (GtkType	  type,
				     GtkType	  is_a_type);
void	 gtk_type_get_arg	    (GtkObject	 *object,
				     GtkType	  type,
				     GtkArg	 *arg,
				     guint	  arg_id);
void	 gtk_type_set_arg	    (GtkObject	 *object,
				     GtkType	  type,
				     GtkArg	 *arg,
				     guint	  arg_id);
GtkArg*	 gtk_arg_copy		    (GtkArg	 *src_arg,
				     GtkArg	 *dest_arg);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TYPE_UTILS_H__ */
