/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
#ifndef __G_LIB_H__
#define __G_LIB_H__


#include <glibconfig.h>

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


/* glib provides definitions for the extrema of many
 *  of the standard types. These are:
 * G_MINFLOAT
 * G_MAXFLOAT
 * G_MINDOUBLE
 * G_MAXDOUBLE
 * G_MINSHORT
 * G_MAXSHORT
 * G_MININT
 * G_MAXINT
 * G_MINLONG
 * G_MAXLONG
 */

#ifdef HAVE_FLOAT_H

#include <float.h>

#define G_MINFLOAT   FLT_MIN
#define G_MAXFLOAT   FLT_MAX
#define G_MINDOUBLE  DBL_MIN
#define G_MAXDOUBLE  DBL_MAX

#elif HAVE_VALUES_H

#include <values.h>

#define G_MINFLOAT  MINFLOAT
#define G_MAXFLOAT  MAXFLOAT
#define G_MINDOUBLE MINDOUBLE
#define G_MAXDOUBLE MAXDOUBLE

#endif /* HAVE_VALUES_H */


#ifdef HAVE_LIMITS_H

#include <limits.h>

#define G_MINSHORT  SHRT_MIN
#define G_MAXSHORT  SHRT_MAX
#define G_MININT    INT_MIN
#define G_MAXINT    INT_MAX
#define G_MINLONG   LONG_MIN
#define G_MAXLONG   LONG_MAX

#elif HAVE_VALUES_H

#ifdef HAVE_FLOAT_H
#include <values.h>
#endif /* HAVE_FLOAT_H */

#define G_MINSHORT  MINSHORT
#define G_MAXSHORT  MAXSHORT
#define G_MININT    MININT
#define G_MAXINT    MAXINT
#define G_MINLONG   MINLONG
#define G_MAXLONG   MAXLONG

#endif /* HAVE_VALUES_H */


/* Provide definitions for some commonly used macros.
 *  These are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */

#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#ifndef NULL
#define NULL ((void*) 0)
#endif /* NULL */

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif /* MAX */

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif /* MIN */

#ifndef ABS
#define ABS(a)     (((a) < 0) ? -(a) : (a))
#endif /* ABS */

#ifndef CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif /* CLAMP */

#ifndef ATEXIT
#define ATEXIT(proc)   (atexit (proc))
#endif /* ATEXIT */


/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#ifdef __DMALLOC_H__

#define g_new(type,count)        ALLOC(type,count)
#define g_new0(type,count)       CALLOC(type,count)

#else /* __DMALLOC_H__ */

#define g_new(type, count)        \
    ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#define g_new0(type, count)       \
    ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#endif /* __DMALLOC_H__ */

#define g_chunk_new(type, chunk)  \
    ((type *) g_mem_chunk_alloc (chunk))


/* Provide macros for error handling. The "assert" macros will
 *  exit on failur. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  in order to support gcc's __PRETTY_FUNCTION__ capability.
 */

#define g_string(x) #x

#ifdef __GNUC__

#define g_assert(expr) \
     if (!(expr))                                    \
       g_error ("file %s: line %d (%s): \"%s\"",     \
		__FILE__,                            \
		__LINE__,                            \
		__PRETTY_FUNCTION__,                 \
		#expr)

#define g_assert_not_reached() \
     g_error ("file %s: line %d (%s): \"should not be reached\"",     \
	      __FILE__,                                               \
	      __LINE__,                                               \
	      __PRETTY_FUNCTION__)

#define g_return_if_fail(expr) \
     if (!(expr))                                              \
       {                                                       \
         g_warning ("file %s: line %d (%s): \"%s\"",           \
		    __FILE__,                                  \
		    __LINE__,                                  \
		    __PRETTY_FUNCTION__,                       \
		    #expr);                                    \
         return;                                               \
       }

#define g_return_val_if_fail(expr,val) \
     if (!(expr))                                              \
       {                                                       \
         g_warning ("file %s: line %d (%s): \"%s\"",           \
		    __FILE__,                                  \
		    __LINE__,                                  \
		    __PRETTY_FUNCTION__,                       \
		    #expr);                                    \
         return val;                                           \
       }

#else /* __GNUC__ */

#define g_assert(expr) \
     if (!(expr))                                    \
       g_error ("file %s: line %d: \"%s\"",          \
		__FILE__,                            \
		__LINE__,                            \
		#expr)

#define g_assert_not_reached() \
     g_error ("file %s: line %d: \"should not be reached\"",          \
	      __FILE__,                                               \
	      __LINE__)

#define g_return_if_fail(expr) \
     if (!(expr))                                        \
       {                                                 \
         g_warning ("file %s: line %d: \"%s\"",          \
		    __FILE__,                            \
		    __LINE__,                            \
		    #expr);                              \
	 return;				         \
       }

#define g_return_val_if_fail(expr, val) \
     if (!(expr))                                        \
       {                                                 \
         g_warning ("file %s: line %d: \"%s\"",          \
		    __FILE__,                            \
		    __LINE__,                            \
		    #expr);                              \
	 return val;	 			         \
       }

#endif /* __GNUC__ */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Provide type definitions for commonly used types.
 *  These are useful because a "gint8" can be adjusted
 *  to be 1 byte (8 bits) on all platforms. Similarly and
 *  more importantly, "gint32" can be adjusted to be
 *  4 bytes (32 bits) on all platforms.
 */

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef char   gboolean;

typedef unsigned char   guchar;
typedef unsigned short  gushort;
typedef unsigned long   gulong;
typedef unsigned int    guint;

typedef float   gfloat;
typedef double  gdouble;

#ifdef HAVE_LONG_DOUBLE
typedef long double gldouble;
#else /* HAVE_LONG_DOUBLE */
typedef double gldouble;
#endif /* HAVE_LONG_DOUBLE */

typedef void* gpointer;

#if (SIZEOF_CHAR == 1)
typedef signed char     gint8;
typedef unsigned char   guint8;
#endif /* SIZEOF_CHAR */


#if (SIZEOF_SHORT == 2)
typedef signed short    gint16;
typedef unsigned short  guint16;
#endif /* SIZEOF_SHORT */


#if (SIZEOF_INT == 4)
typedef signed int      gint32;
typedef unsigned int    guint32;
#elif (SIZEOF_LONG == 4)
typedef signed long     gint32;
typedef unsigned long   guint32;
#endif /* SIZEOF_INT */


typedef struct _GList          GList;
typedef struct _GSList         GSList;
typedef struct _GHashTable     GHashTable;
typedef struct _GCache         GCache;
typedef struct _GTree          GTree;
typedef struct _GTimer         GTimer;
typedef struct _GMemChunk      GMemChunk;
typedef struct _GListAllocator GListAllocator;
typedef struct _GStringChunk   GStringChunk;
typedef struct _GString        GString;
typedef struct _GArray         GArray;

typedef void (*GFunc) (gpointer data, gpointer user_data);
typedef void (*GHFunc) (gpointer key, gpointer value, gpointer user_data);
typedef guint (*GHashFunc) (gpointer key);
typedef gint (*GCompareFunc) (gpointer a, gpointer b);
typedef gpointer (*GCacheNewFunc) (gpointer key);
typedef gpointer (*GCacheDupFunc) (gpointer value);
typedef void (*GCacheDestroyFunc) (gpointer value);
typedef gint (*GTraverseFunc) (gpointer key,
			       gpointer value,
			       gpointer data);
typedef gint (*GSearchFunc) (gpointer key,
			     gpointer data);
typedef void (*GErrorFunc) (gchar *str);
typedef void (*GWarningFunc) (gchar *str);
typedef void (*GPrintFunc) (gchar *str);


struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GSList
{
  gpointer data;
  GSList *next;
};

struct _GString
{
  gchar *str;
  gint len;
};

struct _GArray
{
  gchar *data;
  guint len;
};

struct _GHashTable { gint dummy; };
struct _GCache { gint dummy; };
struct _GTree { gint dummy; };
struct _GTimer { gint dummy; };
struct _GMemChunk { gint dummy; };
struct _GListAllocator { gint dummy; };
struct _GStringChunk { gint dummy; };

typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER
} GTraverseType;

/* Doubly linked lists
 */
GList* g_list_alloc       (void);
void   g_list_free        (GList     *list);
void   g_list_free_1      (GList     *list);
GList* g_list_append      (GList     *list,
			   gpointer   data);
GList* g_list_prepend     (GList     *list,
			   gpointer   data);
GList* g_list_insert      (GList     *list,
			   gpointer   data,
			   gint       position);
GList* g_list_concat      (GList     *list1, 
			   GList     *list2);
GList* g_list_remove      (GList     *list,
			   gpointer   data);
GList* g_list_remove_link (GList     *list,
			   GList     *link);
GList* g_list_reverse     (GList     *list);
GList* g_list_nth         (GList     *list,
			   gint       n);
GList* g_list_find        (GList     *list,
			   gpointer   data);
GList* g_list_last        (GList     *list);
GList* g_list_first       (GList     *list);
gint   g_list_length      (GList     *list);
void   g_list_foreach     (GList     *list,
			   GFunc      func,
			   gpointer   user_data);


/* Singly linked lists
 */
GSList* g_slist_alloc       (void);
void    g_slist_free        (GSList   *list);
void    g_slist_free_1      (GSList   *list);
GSList* g_slist_append      (GSList   *list,
			     gpointer  data);
GSList* g_slist_prepend     (GSList   *list,
			     gpointer  data);
GSList* g_slist_insert      (GSList   *list,
			     gpointer  data,
			     gint      position);
GSList* g_slist_concat      (GSList   *list1, 
			     GSList   *list2);
GSList* g_slist_remove      (GSList   *list,
			     gpointer  data);
GSList* g_slist_remove_link (GSList   *list,
			     GSList   *link);
GSList* g_slist_reverse     (GSList   *list);
GSList* g_slist_nth         (GSList   *list,
			     gint      n);
GSList* g_slist_find        (GSList   *list,
			     gpointer  data);
GSList* g_slist_last        (GSList   *list);
gint    g_slist_length      (GSList   *list);
void    g_slist_foreach     (GSList   *list,
			     GFunc     func,
			     gpointer  user_data);


/* List Allocators
 */
GListAllocator* g_list_allocator_new  (void);
void            g_list_allocator_free (GListAllocator* allocator);
GListAllocator* g_slist_set_allocator (GListAllocator* allocator);
GListAllocator* g_list_set_allocator  (GListAllocator* allocator);


/* Hash tables
 */
GHashTable* g_hash_table_new     (GHashFunc       hash_func,
				  GCompareFunc    key_compare_func);
void        g_hash_table_destroy (GHashTable     *hash_table);
void        g_hash_table_insert  (GHashTable     *hash_table,
				  gpointer        key,
				  gpointer        value);
void        g_hash_table_remove  (GHashTable     *hash_table,
				  gpointer        key);
gpointer    g_hash_table_lookup  (GHashTable     *hash_table,
				  const gpointer  key);
void        g_hash_table_freeze  (GHashTable     *hash_table);
void        g_hash_table_thaw    (GHashTable     *hash_table);
void        g_hash_table_foreach (GHashTable     *hash_table,
				  GHFunc          func,
				  gpointer        user_data);


/* Caches
 */
GCache*  g_cache_new           (GCacheNewFunc      value_new_func,
				GCacheDestroyFunc  value_destroy_func,
				GCacheDupFunc      key_dup_func,
				GCacheDestroyFunc  key_destroy_func,
				GHashFunc          hash_key_func,
				GHashFunc          hash_value_func,
				GCompareFunc       key_compare_func);
void     g_cache_destroy       (GCache            *cache);
gpointer g_cache_insert        (GCache            *cache,
				gpointer           key);
void     g_cache_remove        (GCache            *cache,
				gpointer           value);
void     g_cache_key_foreach   (GCache            *cache,
				GHFunc             func,
				gpointer           user_data);
void     g_cache_value_foreach (GCache            *cache,
				GHFunc             func,
				gpointer           user_data);


/* Trees
 */
GTree*   g_tree_new      (GCompareFunc   key_compare_func);
void     g_tree_destroy  (GTree         *tree);
void     g_tree_insert   (GTree         *tree,
			  gpointer       key,
			  gpointer       value);
void     g_tree_remove   (GTree         *tree,
			  gpointer       key);
gpointer g_tree_lookup   (GTree         *tree,
			  gpointer       key);
void     g_tree_traverse (GTree         *tree,
			  GTraverseFunc  traverse_func,
			  GTraverseType  traverse_type,
			  gpointer       data);
gpointer g_tree_search   (GTree         *tree,
			  GSearchFunc    search_func,
			  gpointer       data);
gint     g_tree_height   (GTree         *tree);
gint     g_tree_nnodes   (GTree         *tree);


/* Memory
 */

#ifdef USE_DMALLOC

#define g_malloc(size)       (gpointer) MALLOC(size)
#define g_malloc0(size)      (gpointer) CALLOC(char,size)
#define g_realloc(mem,size)  (gpointer) REALLOC(mem,char,size)
#define g_free(mem)          FREE(mem)

#else /* USE_DMALLOC */

gpointer g_malloc      (gulong    size);
gpointer g_malloc0     (gulong    size);
gpointer g_realloc     (gpointer  mem,
			gulong    size);
void     g_free        (gpointer  mem);

#endif /* USE_DMALLOC */

void     g_mem_profile (void);
void     g_mem_check   (gpointer  mem);


/* "g_mem_chunk_new" creates a new memory chunk.
 * Memory chunks are used to allocate pieces of memory which are
 *  always the same size. Lists are a good example of such a data type.
 * The memory chunk allocates and frees blocks of memory as needed.
 *  Just be sure to call "g_mem_chunk_free" and not "g_free" on data
 *  allocated in a mem chunk. ("g_free" will most likely cause a seg
 *  fault...somewhere).
 *
 * Oh yeah, GMemChunk is an opaque data type. (You don't really
 *  want to know what's going on inside do you?)
 */

/* ALLOC_ONLY MemChunk's can only allocate memory. The free operation
 *  is interpreted as a no op. ALLOC_ONLY MemChunk's save 4 bytes per
 *  atom. (They are also useful for lists which use MemChunk to allocate
 *  memory but are also part of the MemChunk implementation).
 * ALLOC_AND_FREE MemChunk's can allocate and free memory.
 */

#define G_ALLOC_ONLY      1
#define G_ALLOC_AND_FREE  2

GMemChunk* g_mem_chunk_new     (gchar     *name,
				gint       atom_size,
				gulong     area_size,
				gint       type);
void       g_mem_chunk_destroy (GMemChunk *mem_chunk);
gpointer   g_mem_chunk_alloc   (GMemChunk *mem_chunk);
void       g_mem_chunk_free    (GMemChunk *mem_chunk,
				gpointer   mem);
void       g_mem_chunk_clean   (GMemChunk *mem_chunk);
void       g_mem_chunk_reset   (GMemChunk *mem_chunk);
void       g_mem_chunk_print   (GMemChunk *mem_chunk);
void       g_mem_chunk_info    (void);

/* Ah yes...we have a "g_blow_chunks" function.
 * "g_blow_chunks" simply compresses all the chunks. This operation
 *  consists of freeing every memory area that should be freed (but
 *  which we haven't gotten around to doing yet). And, no,
 *  "g_blow_chunks" doesn't follow the naming scheme, but it is a
 *  much better name than "g_mem_chunk_clean_all" or something
 *  similar.
 */
void g_blow_chunks (void);


/* Timer
 */
GTimer* g_timer_new     (void);
void    g_timer_destroy (GTimer  *timer);
void    g_timer_start   (GTimer  *timer);
void    g_timer_stop    (GTimer  *timer);
void    g_timer_reset   (GTimer  *timer);
gdouble g_timer_elapsed (GTimer  *timer,
			 gulong  *microseconds);


/* Output
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
void g_error   (gchar *format, ...) __attribute__ ((format (printf, 1, 2)));
void g_warning (gchar *format, ...) __attribute__ ((format (printf, 1, 2)));
void g_message (gchar *format, ...) __attribute__ ((format (printf, 1, 2)));
void g_print   (gchar *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
void g_error   (gchar *format, ...);
void g_warning (gchar *format, ...);
void g_message (gchar *format, ...);
void g_print   (gchar *format, ...);
#endif

/* Utility functions
 */
gchar* g_strdup    (const gchar *str);
gchar* g_strerror  (gint errnum);
gchar* g_strsignal (gint signum);


/* Errors
 */
GErrorFunc   g_set_error_handler   (GErrorFunc   func);
GWarningFunc g_set_warning_handler (GWarningFunc func);
GPrintFunc   g_set_message_handler (GPrintFunc func);
GPrintFunc   g_set_print_handler   (GPrintFunc func);

void g_debug          (char *progname);
void g_attach_process (char *progname, int query);
void g_stack_trace    (char *progname, int query);


/* String Chunks
 */
GStringChunk* g_string_chunk_new           (gint size);
void          g_string_chunk_free          (GStringChunk *chunk);
gchar*        g_string_chunk_insert        (GStringChunk *chunk,
					    gchar*        string);
gchar*        g_string_chunk_insert_const  (GStringChunk *chunk,
					    gchar*        string);

/* Strings
 */
GString* g_string_new       (gchar   *init);
void     g_string_free      (GString *string,
			     gint     free_segment);
GString* g_string_assign    (GString *lval,
			     gchar   *rval);
GString* g_string_truncate  (GString *string,
			     gint     len);
GString* g_string_append    (GString *string,
			     gchar   *val);
GString* g_string_append_c  (GString *string,
			     gchar    c);
GString* g_string_prepend   (GString *string,
			     gchar   *val);
GString* g_string_prepend_c (GString *string,
			     gchar    c);
void     g_string_sprintf   (GString *string,
			     gchar   *fmt,
			     ...);
void     g_string_sprintfa  (GString *string,
			     gchar   *fmt,
			     ...);

/* Resizable arrays
 */
#define g_array_append_val(array,type,val) \
     g_rarray_append (array, (gpointer) &val, sizeof (type))
#define g_array_append_vals(array,type,vals,nvals) \
     g_rarray_append (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_prepend_val(array,type,val) \
     g_rarray_prepend (array, (gpointer) &val, sizeof (type))
#define g_array_prepend_vals(array,type,vals,nvals) \
     g_rarray_prepend (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_truncate(array,type,length) \
     g_rarray_truncate (array, length, sizeof (type))
#define g_array_index(array,type,index) \
     ((type*) array->data)[index]

GArray* g_array_new       (gint      zero_terminated);
void    g_array_free      (GArray   *array,
			   gint      free_segment);
GArray* g_rarray_append   (GArray   *array,
			   gpointer  data,
			   gint      size);
GArray* g_rarray_prepend  (GArray   *array,
			   gpointer  data,
			   gint      size);
GArray* g_rarray_truncate (GArray   *array,
			   gint      length,
			   gint      size);


/* Hash Functions
 */
gint  g_string_equal (gpointer v,
		      gpointer v2);
guint g_string_hash  (gpointer v);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __G_LIB_H__ */
