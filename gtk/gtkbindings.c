/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkBindingSet: Keybinding manager for GtkObjects.
 * Copyright (C) 1998 Tim Janik
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

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "gtkbindings.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkrc.h"


/* --- defines --- */
#define	BINDING_MOD_MASK()	(gtk_accelerator_get_default_mod_mask () | GDK_RELEASE_MASK)


/* --- variables --- */
static GHashTable	*binding_entry_hash_table = NULL;
static GSList		*binding_set_list = NULL;
static const gchar	*key_class_binding_set = "gtk-class-binding-set";
static GQuark		 key_id_class_binding_set = 0;


/* --- functions --- */
static GtkBindingSignal*
binding_signal_new (const gchar *signal_name,
		    guint	 n_args)
{
  GtkBindingSignal *signal;
  
  signal = g_new (GtkBindingSignal, 1);
  signal->next = NULL;
  signal->signal_name = g_strdup (signal_name);
  signal->n_args = n_args;
  signal->args = g_new0 (GtkBindingArg, n_args);
  
  return signal;
}

static void
binding_signal_free (GtkBindingSignal *sig)
{
  guint i;
  
  for (i = 0; i < sig->n_args; i++)
    {
      if (GTK_FUNDAMENTAL_TYPE (sig->args[i].arg_type) == GTK_TYPE_STRING)
	g_free (sig->args[i].d.string_data);
    }
  g_free (sig->args);
  g_free (sig->signal_name);
  g_free (sig);
}

static guint
binding_entry_hash (gconstpointer  key)
{
  register const GtkBindingEntry *e = key;
  register guint h;

  h = e->keyval;
  h ^= e->modifiers;

  return h;
}

static gint
binding_entries_compare (gconstpointer  a,
			 gconstpointer  b)
{
  register const GtkBindingEntry *ea = a;
  register const GtkBindingEntry *eb = b;

  return (ea->keyval == eb->keyval && ea->modifiers == eb->modifiers);
}

static GtkBindingEntry*
binding_entry_new (GtkBindingSet *binding_set,
		   guint          keyval,
		   guint          modifiers)
{
  GtkBindingEntry *entry;
  
  if (!binding_entry_hash_table)
    binding_entry_hash_table = g_hash_table_new (binding_entry_hash, binding_entries_compare);

  entry = g_new (GtkBindingEntry, 1);
  entry->keyval = keyval;
  entry->modifiers = modifiers;
  entry->binding_set = binding_set,
  entry->destroyed = FALSE;
  entry->in_emission = FALSE;
  entry->signals = NULL;

  entry->set_next = binding_set->entries;
  binding_set->entries = entry;

  entry->hash_next = g_hash_table_lookup (binding_entry_hash_table, entry);
  g_hash_table_freeze (binding_entry_hash_table);
  if (entry->hash_next)
    g_hash_table_remove (binding_entry_hash_table, entry->hash_next);
  g_hash_table_insert (binding_entry_hash_table, entry, entry);
  g_hash_table_thaw (binding_entry_hash_table);
  
  return entry;
}

static void
binding_entry_free (GtkBindingEntry *entry)
{
  GtkBindingSignal *sig;

  g_assert (entry->set_next == NULL &&
	    entry->hash_next == NULL &&
	    entry->in_emission == FALSE &&
	    entry->destroyed == TRUE);

  entry->destroyed = FALSE;
  
  sig = entry->signals;
  while (sig)
    {
      GtkBindingSignal *prev;
      
      prev = sig;
      sig = prev->next;
      binding_signal_free (prev);
    }
  g_free (entry);
}

static void
binding_entry_destroy (GtkBindingEntry *entry)
{
  GtkBindingEntry *o_entry;
  register GtkBindingEntry *tmp;
  GtkBindingEntry *begin;
  register GtkBindingEntry *last;

  /* unlink from binding set
   */
  last = NULL;
  tmp = entry->binding_set->entries;
  while (tmp)
    {
      if (tmp == entry)
	{
	  if (last)
	    last->set_next = entry->set_next;
	  else
	    entry->binding_set->entries = entry->set_next;
	  break;
	}
      last = tmp;
      tmp = last->set_next;
    }
  entry->set_next = NULL;
  
  o_entry = g_hash_table_lookup (binding_entry_hash_table, entry);
  begin = o_entry;
  last = NULL;
  tmp = begin;
  while (tmp)
    {
      if (tmp == entry)
	{
	  if (last)
	    last->hash_next = entry->hash_next;
	  else
	    begin = entry->hash_next;
	  break;
	}
      last = tmp;
      tmp = last->hash_next;
    }
  entry->hash_next = NULL;
  
  if (!begin)
    g_hash_table_remove (binding_entry_hash_table, entry);
  else if (begin != o_entry)
    {
      g_hash_table_freeze (binding_entry_hash_table);
      g_hash_table_remove (binding_entry_hash_table, entry);
      g_hash_table_insert (binding_entry_hash_table, begin, begin);
      g_hash_table_thaw (binding_entry_hash_table);
    }

  entry->destroyed = TRUE;

  if (!entry->in_emission)
    binding_entry_free (entry);
}

static GtkBindingEntry*
binding_ht_lookup_list (guint keyval,
			guint modifiers)
{
  GtkBindingEntry lookup_entry = { 0 };
  
  if (!binding_entry_hash_table)
    return NULL;
  
  lookup_entry.keyval = keyval;
  lookup_entry.modifiers = modifiers;
  
  return g_hash_table_lookup (binding_entry_hash_table, &lookup_entry);
}

static GtkBindingEntry*
binding_ht_lookup_entry (GtkBindingSet *set,
			 guint		keyval,
			 guint		modifiers)
{
  GtkBindingEntry lookup_entry = { 0 };
  GtkBindingEntry *entry;
  
  if (!binding_entry_hash_table)
    return NULL;
  
  lookup_entry.keyval = keyval;
  lookup_entry.modifiers = modifiers;
  
  entry = g_hash_table_lookup (binding_entry_hash_table, &lookup_entry);
  for (; entry; entry = entry->hash_next)
    if (entry->binding_set == set)
      return entry;

  return NULL;
}

static gboolean
binding_compose_params (GtkBindingArg	*args,
			GtkSignalQuery	*query,
			GtkArg		**params_p)
{
  GtkArg *params;
  const GtkType *types;
  guint i;
  gboolean valid;
  
  params = g_new0 (GtkArg, query->nparams);
  *params_p = params;
  
  types = query->params;
  valid = TRUE;
  for (i = 0; i < query->nparams && valid; i++)
    {
      GtkType param_ftype;

      params->type = *types;
      params->name = NULL;
      param_ftype = GTK_FUNDAMENTAL_TYPE (params->type);
      switch (GTK_FUNDAMENTAL_TYPE (args->arg_type))
	{
	case  GTK_TYPE_DOUBLE:
	  if (param_ftype == GTK_TYPE_FLOAT)
	    GTK_VALUE_FLOAT (*params) = args->d.double_data;
	  else if (param_ftype == GTK_TYPE_DOUBLE)
	    GTK_VALUE_DOUBLE (*params) = args->d.double_data;
	  else
	    valid = FALSE;
	  break;
	case  GTK_TYPE_LONG:
	  if (param_ftype == GTK_TYPE_BOOL &&
	      (args->d.long_data == 0 ||
	       args->d.long_data == 1))
	    GTK_VALUE_BOOL (*params) = args->d.long_data;
	  else if (param_ftype == GTK_TYPE_INT ||
		   param_ftype == GTK_TYPE_ENUM)
	    GTK_VALUE_INT (*params) = args->d.long_data;
	  else if ((param_ftype == GTK_TYPE_UINT ||
		    param_ftype == GTK_TYPE_FLAGS) &&
		   args->d.long_data >= 0)
	    GTK_VALUE_UINT (*params) = args->d.long_data;
	  else if (param_ftype == GTK_TYPE_LONG)
	    GTK_VALUE_LONG (*params) = args->d.long_data;
	  else if (param_ftype == GTK_TYPE_ULONG &&
		   args->d.long_data >= 0)
	    GTK_VALUE_ULONG (*params) = args->d.long_data;
	  else if (param_ftype == GTK_TYPE_FLOAT)
	    GTK_VALUE_FLOAT (*params) = args->d.long_data;
	  else if (param_ftype == GTK_TYPE_DOUBLE)
	    GTK_VALUE_DOUBLE (*params) = args->d.long_data;
	  else
	    valid = FALSE;
	  break;
	case  GTK_TYPE_STRING:
	  if (args->arg_type == GTK_TYPE_STRING &&
	      param_ftype == GTK_TYPE_STRING)
	    GTK_VALUE_STRING (*params) = args->d.string_data;
	  else if (args->arg_type == GTK_TYPE_IDENTIFIER &&
		   (param_ftype == GTK_TYPE_ENUM ||
		    param_ftype == GTK_TYPE_FLAGS))
	    {
	      GtkEnumValue *value;

	      value = gtk_type_enum_find_value (params->type, args->d.string_data);
	      if (value)
		GTK_VALUE_ENUM (*params) = value->value;
	      else
		valid = FALSE;
	    }
	  else
	    valid = FALSE;
	  break;
	default:
	  valid = FALSE;
	  break;
	}
      types++;
      params++;
      args++;
    }
  
  if (!valid)
    {
      g_free (*params_p);
      *params_p = NULL;
    }
  
  return valid;
}

static void
gtk_binding_entry_activate (GtkBindingEntry	*entry,
			    GtkObject	*object)
{
  GtkBindingSignal *sig;
  gboolean old_emission;
  
  old_emission = entry->in_emission;
  entry->in_emission = TRUE;
  
  gtk_object_ref (object);
  
  for (sig = entry->signals; sig; sig = sig->next)
    {
      GtkSignalQuery *query;
      guint signal_id;
      GtkArg *params = NULL;
      gchar *accelerator = NULL;
      
      signal_id = gtk_signal_lookup (sig->signal_name, GTK_OBJECT_TYPE (object));
      if (!signal_id)
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "could not find signal \"%s\" in the `%s' class ancestry",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     gtk_type_name (GTK_OBJECT_TYPE (object)));
	  g_free (accelerator);
	  continue;
	}
      
      query = gtk_signal_query (signal_id);
      if (query->nparams != sig->n_args ||
	  query->return_val != GTK_TYPE_NONE ||
	  !binding_compose_params (sig->args, query, &params))
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "signature mismatch for signal \"%s\" in the `%s' class ancestry",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     gtk_type_name (GTK_OBJECT_TYPE (object)));
	}
      else if (!(query->signal_flags & GTK_RUN_ACTION))
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "signal \"%s\" in the `%s' class ancestry cannot be used for action emissions",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     gtk_type_name (GTK_OBJECT_TYPE (object)));
	}
      g_free (accelerator);
      g_free (query);
      if (accelerator)
	continue;

      gtk_signal_emitv (object, signal_id, params);
      g_free (params);
      
      if (GTK_OBJECT_DESTROYED (object) || entry->destroyed)
	break;
    }
  
  gtk_object_unref (object);

  entry->in_emission = old_emission;
  if (entry->destroyed && !entry->in_emission)
    binding_entry_free (entry);
}

GtkBindingSet*
gtk_binding_set_new (const gchar    *set_name)
{
  GtkBindingSet *binding_set;
  
  g_return_val_if_fail (set_name != NULL, NULL);
  
  binding_set = g_new (GtkBindingSet, 1);
  binding_set->set_name = g_strdup (set_name);
  binding_set->widget_path_pspecs = NULL;
  binding_set->widget_class_pspecs = NULL;
  binding_set->class_branch_pspecs = NULL;
  binding_set->entries = NULL;
  binding_set->current = NULL;
  
  binding_set_list = g_slist_prepend (binding_set_list, binding_set);
  
  return binding_set;
}

GtkBindingSet*
gtk_binding_set_by_class (gpointer object_class)
{
  GtkObjectClass *class = object_class;
  GtkBindingSet* binding_set;

  g_return_val_if_fail (GTK_IS_OBJECT_CLASS (class), NULL);

  if (!key_id_class_binding_set)
    key_id_class_binding_set = g_quark_from_static_string (key_class_binding_set);

  binding_set = g_dataset_id_get_data (class, key_id_class_binding_set);

  if (binding_set)
    return binding_set;

  binding_set = gtk_binding_set_new (gtk_type_name (class->type));
  gtk_binding_set_add_path (binding_set,
			    GTK_PATH_CLASS,
			    gtk_type_name (class->type),
			    GTK_PATH_PRIO_GTK);
  g_dataset_id_set_data (class, key_id_class_binding_set, binding_set);

  return binding_set;
}

GtkBindingSet*
gtk_binding_set_find (const gchar    *set_name)
{
  GSList *slist;
  
  g_return_val_if_fail (set_name != NULL, NULL);
  
  for (slist = binding_set_list; slist; slist = slist->next)
    {
      GtkBindingSet *binding_set;
      
      binding_set = slist->data;
      if (g_str_equal (binding_set->set_name, (gpointer) set_name))
	return binding_set;
    }
  return NULL;
}

gboolean
gtk_binding_set_activate (GtkBindingSet	 *binding_set,
			  guint		  keyval,
			  guint		  modifiers,
			  GtkObject	 *object)
{
  GtkBindingEntry *entry;
  
  g_return_val_if_fail (binding_set != NULL, FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  if (!GTK_OBJECT_DESTROYED (object))
    {
      entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
      if (entry)
	{
	  gtk_binding_entry_activate (entry, object);
	  
	  return TRUE;
	}
    }
  
  return FALSE;
}

void
gtk_binding_entry_clear (GtkBindingSet	*binding_set,
			 guint		 keyval,
			 guint		 modifiers)
{
  GtkBindingEntry *entry;
  
  g_return_if_fail (binding_set != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);

  entry = binding_entry_new (binding_set, keyval, modifiers);
}

void
gtk_binding_entry_remove (GtkBindingSet	 *binding_set,
			  guint		  keyval,
			  guint		  modifiers)
{
  GtkBindingEntry *entry;
  
  g_return_if_fail (binding_set != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);
}

void
gtk_binding_entry_add_signall (GtkBindingSet  *binding_set,
			       guint	       keyval,
			       guint	       modifiers,
			       const gchar    *signal_name,
			       GSList	      *binding_args)
{
  GtkBindingEntry *entry;
  GtkBindingSignal *signal, **signal_p;
  GSList *slist;
  guint n = 0;
  GtkBindingArg *arg;
  
  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (signal_name != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  signal = binding_signal_new (signal_name, g_slist_length (binding_args));
  
  arg = signal->args;
  for (slist = binding_args; slist; slist = slist->next)
    {
      GtkBindingArg *tmp_arg;
      
      tmp_arg = slist->data;
      if (!tmp_arg)
	{
	  g_warning ("gtk_binding_entry_add_signall(): arg[%u] is `NULL'", n);
	  binding_signal_free (signal);
	  return;
	}
      switch (GTK_FUNDAMENTAL_TYPE (tmp_arg->arg_type))
	{
	case  GTK_TYPE_LONG:
	  arg->arg_type = GTK_TYPE_LONG;
	  arg->d.long_data = tmp_arg->d.long_data;
	  break;
	case  GTK_TYPE_DOUBLE:
	  arg->arg_type = GTK_TYPE_DOUBLE;
	  arg->d.double_data = tmp_arg->d.double_data;
	  break;
	case  GTK_TYPE_STRING:
          if (tmp_arg->arg_type != GTK_TYPE_IDENTIFIER)
	    arg->arg_type = GTK_TYPE_STRING;
	  else
	    arg->arg_type = GTK_TYPE_IDENTIFIER;
	  arg->d.string_data = g_strdup (tmp_arg->d.string_data);
	  if (!arg->d.string_data)
	    {
	      g_warning ("gtk_binding_entry_add_signall(): value of `string' arg[%u] is `NULL'", n);
	      binding_signal_free (signal);
	      return;
	    }
	  break;
	default:
	  g_warning ("gtk_binding_entry_add_signall(): unsupported type `%s' for arg[%u]",
		     gtk_type_name (arg->arg_type), n);
	  binding_signal_free (signal);
	  return;
	}
      arg++;
      n++;
    }
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (!entry)
    {
      gtk_binding_entry_add (binding_set, keyval, modifiers);
      entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
    }
  signal_p = &entry->signals;
  while (*signal_p)
    signal_p = &(*signal_p)->next;
  *signal_p = signal;
}

void
gtk_binding_entry_add_signal (GtkBindingSet  *binding_set,
			      guint           keyval,
			      guint           modifiers,
			      const gchar    *signal_name,
			      guint           n_args,
			      ...)
{
  GSList *slist, *free_slist;
  va_list args;
  guint i;

  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (signal_name != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  va_start (args, n_args);
  slist = NULL;
  for (i = 0; i < n_args; i++)
    {
      GtkBindingArg *arg;

      arg = g_new0 (GtkBindingArg, 1);
      slist = g_slist_prepend (slist, arg);

      arg->arg_type = va_arg (args, GtkType);
      switch (GTK_FUNDAMENTAL_TYPE (arg->arg_type))
	{
	  /* for elaborated commenting about var args collection, take a look
	   * at gtk_arg_collect_value() in gtkargcollector.c
	   */
	case GTK_TYPE_CHAR:
	case GTK_TYPE_UCHAR:
	case GTK_TYPE_INT:
	case GTK_TYPE_UINT:
	case GTK_TYPE_BOOL:
	case GTK_TYPE_ENUM:
	case GTK_TYPE_FLAGS:
	  arg->arg_type = GTK_TYPE_LONG;
	  arg->d.long_data = va_arg (args, gint);
	  break;
	case GTK_TYPE_LONG:
	case GTK_TYPE_ULONG:
	  arg->arg_type = GTK_TYPE_LONG;
	  arg->d.long_data = va_arg (args, glong);
	  break;
	case GTK_TYPE_FLOAT:
	case GTK_TYPE_DOUBLE:
	  arg->arg_type = GTK_TYPE_DOUBLE;
	  arg->d.double_data = va_arg (args, gdouble);
	  break;
	case GTK_TYPE_STRING:
	  if (arg->arg_type != GTK_TYPE_IDENTIFIER)
	    arg->arg_type = GTK_TYPE_STRING;
	  arg->d.string_data = va_arg (args, gchar*);
	  if (!arg->d.string_data)
	    {
	      g_warning ("gtk_binding_entry_add_signal(): type `%s' arg[%u] is `NULL'",
			 gtk_type_name (arg->arg_type),
			 i);
	      i += n_args + 1;
	    }
	  break;
	default:
	  g_warning ("gtk_binding_entry_add_signal(): unsupported type `%s' for arg[%u]",
		     gtk_type_name (arg->arg_type), i);
	  i += n_args + 1;
	  break;
	}
    }
  va_end (args);

  if (i == n_args || i == 0)
    {
      slist = g_slist_reverse (slist);
      gtk_binding_entry_add_signall (binding_set, keyval, modifiers, signal_name, slist);
    }

  free_slist = slist;
  while (slist)
    {
      g_free (slist->data);
      slist = slist->next;
    }
  g_slist_free (free_slist);
}

void
gtk_binding_set_add_path (GtkBindingSet	     *binding_set,
			  GtkPathType	      path_type,
			  const gchar	     *path_pattern,
			  GtkPathPriorityType priority)
{
  GtkPatternSpec *pspec;
  GSList **slist_p, *slist;
  static guint seq_id = 0;
  
  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (path_pattern != NULL);

  priority &= GTK_PATH_PRIO_MASK;
  
  switch (path_type)
    {
    case  GTK_PATH_WIDGET:
      slist_p = &binding_set->widget_path_pspecs;
      break;
    case  GTK_PATH_WIDGET_CLASS:
      slist_p = &binding_set->widget_class_pspecs;
      break;
    case  GTK_PATH_CLASS:
      slist_p = &binding_set->class_branch_pspecs;
      break;
    default:
      g_assert_not_reached ();
      slist_p = NULL;
      break;
    }
  
  pspec = g_new (GtkPatternSpec, 1);
  gtk_pattern_spec_init (pspec, path_pattern);
  pspec->seq_id = seq_id++ & 0x0fffffff;
  pspec->seq_id |= priority << 28;
  pspec->user_data = binding_set;
  
  slist = *slist_p;
  while (slist)
    {
      GtkPatternSpec *tmp_pspec;
      
      tmp_pspec = slist->data;
      slist = slist->next;
      
      if (tmp_pspec->pattern_length == pspec->pattern_length &&
	  g_str_equal (tmp_pspec->pattern_reversed, pspec->pattern_reversed))
	{
	  gtk_pattern_spec_free_segs (pspec);
	  g_free (pspec);
	  pspec = NULL;
	  break;
	}
    }
  if (pspec)
    *slist_p = g_slist_prepend (*slist_p, pspec);
}

static inline gboolean
binding_match_activate (GSList          *pspec_list,
			GtkObject	*object,
			guint	         path_length,
			gchar	        *path,
			gchar	        *path_reversed)
{
  GSList *slist;

  for (slist = pspec_list; slist; slist = slist->next)
    {
      GtkPatternSpec *pspec;

      pspec = slist->data;
      if (gtk_pattern_match (pspec, path_length, path, path_reversed))
	{
	  GtkBindingSet *binding_set;

	  binding_set = pspec->user_data;

	  gtk_binding_entry_activate (binding_set->current, object);

	  return TRUE;
	}
    }

  return FALSE;
}

static gint
gtk_binding_pattern_compare (gconstpointer new_pattern,
			     gconstpointer existing_pattern)
{
  register const GtkPatternSpec *np  = new_pattern;
  register const GtkPatternSpec *ep  = existing_pattern;

  /* walk the list as long as the existing patterns have
   * higher priorities.
   */

  return np->seq_id < ep->seq_id;
}

static inline GSList*
gtk_binding_entries_sort_patterns (GtkBindingEntry    *entries,
				   GtkPathType         path_id)
{
  GSList *patterns;

  patterns = NULL;
  while (entries)
    {
      register GtkBindingSet *binding_set;
      GSList *slist = NULL;

      binding_set = entries->binding_set;
      binding_set->current = entries;

      switch (path_id)
	{
	case GTK_PATH_WIDGET:
	  slist = binding_set->widget_path_pspecs;
	  break;
	case GTK_PATH_WIDGET_CLASS:
	  slist = binding_set->widget_class_pspecs;
	  break;
	case GTK_PATH_CLASS:
	  slist = binding_set->class_branch_pspecs;
	  break;
	}

      for (; slist; slist = slist->next)
	{
	  GtkPatternSpec *pspec;

	  pspec = slist->data;
	  patterns = g_slist_insert_sorted (patterns, pspec, gtk_binding_pattern_compare);
	}

      entries = entries->hash_next;
    }

  return patterns;
}
      

gboolean
gtk_bindings_activate (GtkObject      *object,
		       guint           keyval,
		       guint           modifiers)
{
  GtkBindingEntry *entries;
  GtkWidget *widget;
  gboolean handled = FALSE;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);

  if (!GTK_IS_WIDGET (object) || GTK_OBJECT_DESTROYED (object))
    return FALSE;

  widget = GTK_WIDGET (object);

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entries = binding_ht_lookup_list (keyval, modifiers);

  if (!entries)
    return FALSE;

  if (!handled)
    {
      guint path_length;
      gchar *path, *path_reversed;
      GSList *patterns;

      gtk_widget_path (widget, &path_length, &path, &path_reversed);
      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_WIDGET);
      handled = binding_match_activate (patterns, object, path_length, path, path_reversed);
      g_slist_free (patterns);
      g_free (path);
      g_free (path_reversed);
    }

  if (!handled)
    {
      guint path_length;
      gchar *path, *path_reversed;
      GSList *patterns;

      gtk_widget_class_path (widget, &path_length, &path, &path_reversed);
      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_WIDGET_CLASS);
      handled = binding_match_activate (patterns, object, path_length, path, path_reversed);
      g_slist_free (patterns);
      g_free (path);
      g_free (path_reversed);
    }

  if (!handled)
    {
      GSList *patterns;
      GtkType class_type;
      
      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_CLASS);
      class_type = GTK_OBJECT_TYPE (object);
      while (class_type && !handled)
	{
	  guint path_length;
	  gchar *path, *path_reversed;
	  
	  path = gtk_type_name (class_type);
	  path_reversed = g_strdup (path);
	  g_strreverse (path_reversed);
	  path_length = strlen (path);
	  handled = binding_match_activate (patterns, object, path_length, path, path_reversed);
	  g_free (path_reversed);

	  class_type = gtk_type_parent (class_type);
	}
      g_slist_free (patterns);
    }

  return handled;
}










/* Patterns
 */

static inline gboolean
gtk_pattern_ph_match (const gchar   *match_pattern,
		      const gchar   *match_string)
{
  register const gchar *pattern, *string;
  register gchar ch;
  
  pattern = match_pattern;
  string = match_string;
  
  ch = *pattern;
  pattern++;
  while (ch)
    {
      switch (ch)
	{
	case  '?':
	  if (!*string)
	    return FALSE;
	  string++;
	  break;
	  
	case  '*':
	  do
	    {
	      ch = *pattern;
	      pattern++;
	      if (ch == '?')
		{
		  if (!*string)
		    return FALSE;
		  string++;
		}
	    }
	  while (ch == '*' || ch == '?');
	  if (!ch)
	    return TRUE;
	  do
	    {
	      while (ch != *string)
		{
		  if (!*string)
		    return FALSE;
		  string++;
		}
	      string++;
	      if (gtk_pattern_ph_match (pattern, string))
		return TRUE;
	    }
	  while (*string);
	  break;
	  
	default:
	  if (ch == *string)
	    string++;
	  else
	    return FALSE;
	  break;
	}
      
      ch = *pattern;
      pattern++;
    }
  
  return *string == 0;
}

gboolean
gtk_pattern_match (GtkPatternSpec	*pspec,
		   guint		 string_length,
		   const gchar		*string,
		   const gchar		*string_reversed)
{
  g_return_val_if_fail (pspec != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (string_reversed != NULL, FALSE);
  
  switch (pspec->match_type)
    {
    case  GTK_MATCH_ALL:
      return gtk_pattern_ph_match (pspec->pattern, string);
      
    case  GTK_MATCH_ALL_TAIL:
      return gtk_pattern_ph_match (pspec->pattern_reversed, string_reversed);
      
    case  GTK_MATCH_HEAD:
      if (pspec->pattern_length > string_length)
	return FALSE;
      else if (pspec->pattern_length == string_length)
	return strcmp (pspec->pattern, string) == 0;
      else if (pspec->pattern_length)
	return strncmp (pspec->pattern, string, pspec->pattern_length) == 0;
      else
	return TRUE;
      
    case  GTK_MATCH_TAIL:
      if (pspec->pattern_length > string_length)
	return FALSE;
      else if (pspec->pattern_length == string_length)
	return strcmp (pspec->pattern_reversed, string_reversed) == 0;
      else if (pspec->pattern_length)
	return strncmp (pspec->pattern_reversed,
			string_reversed,
			pspec->pattern_length) == 0;
      else
	return TRUE;
      
    case  GTK_MATCH_EXACT:
      if (pspec->pattern_length != string_length)
	return FALSE;
      else
	return strcmp (pspec->pattern_reversed, string_reversed) == 0;
      
    default:
      g_return_val_if_fail (pspec->match_type < GTK_MATCH_LAST, FALSE);
      return FALSE;
    }
}

void
gtk_pattern_spec_init (GtkPatternSpec	      *pspec,
		       const gchar	      *pattern)
{
  gchar *p;
  
  g_return_if_fail (pspec != NULL);
  
  pspec->match_type = GTK_MATCH_ALL;
  pspec->seq_id = 0;
  pspec->user_data = NULL;
  
  if (!pattern)
    pattern = "";
  
  pspec->pattern = g_strdup (pattern);
  pspec->pattern_length = strlen (pspec->pattern);
  pspec->pattern_reversed = g_strdup (pspec->pattern);
  g_strreverse (pspec->pattern_reversed);
  if (pspec->pattern_reversed[0] != '*')
    pspec->match_type = GTK_MATCH_ALL_TAIL;
  
  if (strchr (pspec->pattern, '?'))
    return;
  
  if (!strchr (pspec->pattern, '*'))
    {
      pspec->match_type = GTK_MATCH_EXACT;
      return;
    }
  
  p = pspec->pattern;
  while (*p == '*')
    p++;
  if (p > pspec->pattern &&
      !strchr (p, '*'))
    {
      gchar *t;
      
      pspec->match_type = GTK_MATCH_TAIL;
      t = pspec->pattern;
      pspec->pattern = g_strdup (p);
      g_free (t);
      g_free (pspec->pattern_reversed);
      pspec->pattern_reversed = g_strdup (pspec->pattern);
      g_strreverse (pspec->pattern_reversed);
      pspec->pattern_length = strlen (pspec->pattern);
      return;
    }
  
  p = pspec->pattern_reversed;
  while (*p == '*')
    p++;
  if (p > pspec->pattern_reversed &&
      !strchr (p, '*'))
    {
      gchar *t;
      
      pspec->match_type = GTK_MATCH_HEAD;
      t = pspec->pattern_reversed;
      pspec->pattern_reversed = g_strdup (p);
      g_free (t);
      g_free (pspec->pattern);
      pspec->pattern = g_strdup (pspec->pattern_reversed);
      g_strreverse (pspec->pattern);
      pspec->pattern_length = strlen (pspec->pattern);
    }
}

gboolean
gtk_pattern_match_string (GtkPatternSpec	*pspec,
			  const gchar		*string)
{
  gchar *string_reversed;
  guint length;
  gboolean ergo;
  
  g_return_val_if_fail (pspec != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  
  length = strlen (string);
  string_reversed = g_strdup (string);
  g_strreverse (string_reversed);
  
  ergo = gtk_pattern_match (pspec, length, string, string_reversed);
  g_free (string_reversed);
  
  return ergo;
}

gboolean
gtk_pattern_match_simple (const gchar		*pattern,
			  const gchar		*string)
{
  GtkPatternSpec pspec;
  gboolean ergo;
  
  g_return_val_if_fail (pattern != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  
  gtk_pattern_spec_init (&pspec, pattern);
  ergo = gtk_pattern_match_string (&pspec, string);
  gtk_pattern_spec_free_segs (&pspec);
  
  return ergo;
}

void
gtk_pattern_spec_free_segs (GtkPatternSpec	*pspec)
{
  g_return_if_fail (pspec != NULL);
  
  g_free (pspec->pattern);
  pspec->pattern = NULL;
  g_free (pspec->pattern_reversed);
  pspec->pattern_reversed = NULL;
}

static guint
gtk_binding_parse_signal (GScanner       *scanner,
			  GtkBindingSet  *binding_set,
			  guint		  keyval,
			  guint		  modifiers)
{
  gchar *signal;
  guint expected_token = 0;
  GSList *args;
  GSList *slist;
  gboolean done;
  gboolean negate;
  gboolean need_arg;
  gboolean seen_comma;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != '(')
    {
      g_scanner_get_next_token (scanner);
      return '(';
    }
  signal = g_strdup (scanner->value.v_string);
  g_scanner_get_next_token (scanner);

  negate = FALSE;
  args = NULL;
  done = FALSE;
  need_arg = TRUE;
  seen_comma = FALSE;
  scanner->config->scan_symbols = FALSE;
  do
    {
      if (need_arg)
	expected_token = G_TOKEN_INT;
      else
	expected_token = ')';
      g_scanner_get_next_token (scanner);
      switch (scanner->token)
	{
	  GtkBindingArg *arg;

	case G_TOKEN_FLOAT:
	  if (need_arg)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = GTK_TYPE_DOUBLE;
	      arg->d.double_data = scanner->value.v_float;
	      if (negate)
		{
		  arg->d.double_data = - arg->d.double_data;
		  negate = FALSE;
		}
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case G_TOKEN_INT:
	  if (need_arg)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = GTK_TYPE_LONG;
	      arg->d.long_data = scanner->value.v_int;
	      if (negate)
		{
		  arg->d.long_data = - arg->d.long_data;
		  negate = FALSE;
		}
	      args = g_slist_prepend (args, arg);
	    }
          else
	    done = TRUE;
	  break;
	case G_TOKEN_STRING:
	  if (need_arg && !negate)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = GTK_TYPE_STRING;
	      arg->d.string_data = g_strdup (scanner->value.v_string);
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case G_TOKEN_IDENTIFIER:
	  if (need_arg && !negate)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = GTK_TYPE_IDENTIFIER;
	      arg->d.string_data = g_strdup (scanner->value.v_identifier);
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case '-':
	  if (!need_arg)
	    done = TRUE;
	  else if (negate)
	    {
	      expected_token = G_TOKEN_INT;
	      done = TRUE;
	    }
	  else
	    negate = TRUE;
	  break;
	case ',':
	  seen_comma = TRUE;
	  if (need_arg)
	    done = TRUE;
	  else
	    need_arg = TRUE;
	  break;
	case ')':
	  if (!(need_arg && seen_comma) && !negate)
	    {
	      args = g_slist_reverse (args);
	      gtk_binding_entry_add_signall (binding_set,
					     keyval,
					     modifiers,
					     signal,
					     args);
	      expected_token = G_TOKEN_NONE;
	    }
	  done = TRUE;
	  break;
	default:
	  done = TRUE;
	  break;
	}
    }
  while (!done);
  scanner->config->scan_symbols = TRUE;
  
  for (slist = args; slist; slist = slist->next)
    {
      GtkBindingArg *arg;

      arg = slist->data;
      if (GTK_FUNDAMENTAL_TYPE (arg->arg_type) == GTK_TYPE_STRING)
	g_free (arg->d.string_data);
      g_free (arg);
    }
  g_slist_free (args);
  g_free (signal);

  return expected_token;
}

static inline guint
gtk_binding_parse_bind (GScanner       *scanner,
			GtkBindingSet  *binding_set)
{
  guint keyval = 0;
  guint modifiers = 0;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != GTK_RC_TOKEN_BIND)
    return GTK_RC_TOKEN_BIND;
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  gtk_accelerator_parse (scanner->value.v_string, &keyval, &modifiers);
  modifiers &= BINDING_MOD_MASK ();
  if (keyval == 0)
    return G_TOKEN_STRING;

  g_scanner_get_next_token (scanner);
  if (scanner->token != '{')
    return '{';

  gtk_binding_entry_clear (binding_set, keyval, modifiers);
  
  g_scanner_peek_next_token (scanner);
  while (scanner->next_token != '}')
    {
      switch (scanner->next_token)
	{
	  guint expected_token;

	case G_TOKEN_STRING:
	  expected_token = gtk_binding_parse_signal (scanner,
						     binding_set,
						     keyval,
						     modifiers);
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  return '}';
	}
      g_scanner_peek_next_token (scanner);
    }
  g_scanner_get_next_token (scanner);

  return G_TOKEN_NONE;
}

guint
gtk_binding_parse_binding (GScanner       *scanner)
{
  gchar *name;
  GtkBindingSet *binding_set;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  g_scanner_get_next_token (scanner);
  if (scanner->token != GTK_RC_TOKEN_BINDING)
    return GTK_RC_TOKEN_BINDING;
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  name = g_strdup (scanner->value.v_string);

  g_scanner_get_next_token (scanner);
  if (scanner->token != '{')
    {
      g_free (name);
      return G_TOKEN_STRING;
    }

  binding_set = gtk_binding_set_find (name);
  if (!binding_set)
    binding_set = gtk_binding_set_new (name);
  g_free (name);

  g_scanner_peek_next_token (scanner);
  while (scanner->next_token != '}')
    {
      switch (scanner->next_token)
	{
	  guint expected_token;

	case GTK_RC_TOKEN_BIND:
	  expected_token = gtk_binding_parse_bind (scanner, binding_set);
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  return '}';
	}
      g_scanner_peek_next_token (scanner);
    }
  g_scanner_get_next_token (scanner);

  return G_TOKEN_NONE;
}

