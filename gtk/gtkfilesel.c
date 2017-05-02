/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include <glib.h>		/* Include early to get G_OS_WIN32 etc */
#include <glib/gstdio.h>

#if defined(G_PLATFORM_WIN32)
#include <ctype.h>
#define STRICT
#include <windows.h>
#undef STRICT
#endif /* G_PLATFORM_WIN32 */

#include "gdk/gdkkeysyms.h"

#undef GTK_DISABLE_DEPRECATED /* GtkOptionMenu */

#include "gtkbutton.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkfilesel.h"
#include "gtkhbox.h"
#include "gtkhbbox.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtkstock.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkvbox.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkdialog.h"
#include "gtkmessagedialog.h"
#include "gtkdnd.h"
#include "gtkeventbox.h"
#include "gtkoptionmenu.h"

#define WANT_HPANED 1
#include "gtkhpaned.h"

#include "gtkalias.h"

#ifdef G_OS_WIN32
#include <direct.h>
#include <io.h>
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#endif /* G_OS_WIN32 */

#ifdef G_WITH_CYGWIN
#include <sys/cygwin.h>		/* For cygwin_conv_to_posix_path */
#endif

#define DIR_LIST_WIDTH   180
#define DIR_LIST_HEIGHT  180
#define FILE_LIST_WIDTH  180
#define FILE_LIST_HEIGHT 180

/* The Hurd doesn't define either PATH_MAX or MAXPATHLEN, so we put this
 * in here, since the rest of the code in the file does require some
 * fixed maximum.
 */
#ifndef MAXPATHLEN
#  ifdef PATH_MAX
#    define MAXPATHLEN PATH_MAX
#  else
#    define MAXPATHLEN 2048
#  endif
#endif

/* I've put this here so it doesn't get confused with the 
 * file completion interface */
typedef struct _HistoryCallbackArg HistoryCallbackArg;

struct _HistoryCallbackArg
{
  gchar *directory;
  GtkWidget *menu_item;
};


typedef struct _CompletionState    CompletionState;
typedef struct _CompletionDir      CompletionDir;
typedef struct _CompletionDirSent  CompletionDirSent;
typedef struct _CompletionDirEntry CompletionDirEntry;
typedef struct _CompletionUserDir  CompletionUserDir;
typedef struct _PossibleCompletion PossibleCompletion;

/* Non-external file completion decls and structures */

/* A contant telling PRCS how many directories to cache.  Its actually
 * kept in a list, so the geometry isn't important. */
#define CMPL_DIRECTORY_CACHE_SIZE 10

/* A constant used to determine whether a substring was an exact
 * match by first_diff_index()
 */
#define PATTERN_MATCH -1
#define CMPL_ERRNO_TOO_LONG ((1<<16)-1)
#define CMPL_ERRNO_DID_NOT_CONVERT ((1<<16)-2)

/* This structure contains all the useful information about a directory
 * for the purposes of filename completion.  These structures are cached
 * in the CompletionState struct.  CompletionDir's are reference counted.
 */
struct _CompletionDirSent
{
#ifndef G_PLATFORM_WIN32
  ino_t inode;
  time_t mtime;
  dev_t device;
#endif

  gint entry_count;
  struct _CompletionDirEntry *entries;
};

struct _CompletionDir
{
  CompletionDirSent *sent;

  gchar *fullname;
  gint fullname_len;

  struct _CompletionDir *cmpl_parent;
  gint cmpl_index;
  gchar *cmpl_text;
};

/* This structure contains pairs of directory entry names with a flag saying
 * whether or not they are a valid directory.  NOTE: This information is used
 * to provide the caller with information about whether to update its completions
 * or try to open a file.  Since directories are cached by the directory mtime,
 * a symlink which points to an invalid file (which will not be a directory),
 * will not be reevaluated if that file is created, unless the containing
 * directory is touched.  I consider this case to be worth ignoring (josh).
 */
struct _CompletionDirEntry
{
  gboolean is_dir;
  gchar *entry_name;
  gchar *sort_key;
};

struct _CompletionUserDir
{
  gchar *login;
  gchar *homedir;
};

struct _PossibleCompletion
{
  /* accessible fields, all are accessed externally by functions
   * declared above
   */
  gchar *text;
  gint is_a_completion;
  gboolean is_directory;

  /* Private fields
   */
  gint text_alloc;
};

struct _CompletionState
{
  gint last_valid_char;
  gchar *updated_text;
  gint updated_text_len;
  gint updated_text_alloc;
  gboolean re_complete;

  gchar *user_dir_name_buffer;
  gint user_directories_len;

  gchar *last_completion_text;

  gint user_completion_index; /* if >= 0, currently completing ~user */

  struct _CompletionDir *completion_dir; /* directory completing from */
  struct _CompletionDir *active_completion_dir;

  struct _PossibleCompletion the_completion;

  struct _CompletionDir *reference_dir; /* initial directory */

  GList* directory_storage;
  GList* directory_sent_storage;

  struct _CompletionUserDir *user_directories;
};

enum {
  PROP_0,
  PROP_SHOW_FILEOPS,
  PROP_FILENAME,
  PROP_SELECT_MULTIPLE
};

enum {
  DIR_COLUMN
};

enum {
  FILE_COLUMN
};

/* File completion functions which would be external, were they used
 * outside of this file.
 */

static CompletionState*    cmpl_init_state        (void);
static void                cmpl_free_state        (CompletionState *cmpl_state);
static gint                cmpl_state_okay        (CompletionState* cmpl_state);
static const gchar*        cmpl_strerror          (gint);

static PossibleCompletion* cmpl_completion_matches(gchar           *text_to_complete,
						   gchar          **remaining_text,
						   CompletionState *cmpl_state);

/* Returns a name for consideration, possibly a completion, this name
 * will be invalid after the next call to cmpl_next_completion.
 */
static char*               cmpl_this_completion   (PossibleCompletion*);

/* True if this completion matches the given text.  Otherwise, this
 * output can be used to have a list of non-completions.
 */
static gint                cmpl_is_a_completion   (PossibleCompletion*);

/* True if the completion is a directory
 */
static gboolean            cmpl_is_directory      (PossibleCompletion*);

/* Obtains the next completion, or NULL
 */
static PossibleCompletion* cmpl_next_completion   (CompletionState*);

/* Updating completions: the return value of cmpl_updated_text() will
 * be text_to_complete completed as much as possible after the most
 * recent call to cmpl_completion_matches.  For the present
 * application, this is the suggested replacement for the user's input
 * string.  You must CALL THIS AFTER ALL cmpl_text_completions have
 * been received.
 */
static gchar*              cmpl_updated_text       (CompletionState* cmpl_state);

/* After updating, to see if the completion was a directory, call
 * this.  If it was, you should consider re-calling completion_matches.
 */
static gboolean            cmpl_updated_dir        (CompletionState* cmpl_state);

/* Current location: if using file completion, return the current
 * directory, from which file completion begins.  More specifically,
 * the cwd concatenated with all exact completions up to the last
 * directory delimiter('/').
 */
static gchar*              cmpl_reference_position (CompletionState* cmpl_state);

#if 0
/* This doesn't work currently and would require changes
 * to fnmatch.c to get working.
 */
/* backing up: if cmpl_completion_matches returns NULL, you may query
 * the index of the last completable character into cmpl_updated_text.
 */
static gint                cmpl_last_valid_char    (CompletionState* cmpl_state);
#endif

/* When the user selects a non-directory, call cmpl_completion_fullname
 * to get the full name of the selected file.
 */
static gchar*              cmpl_completion_fullname (const gchar*, CompletionState* cmpl_state);


/* Directory operations. */
static CompletionDir* open_ref_dir         (gchar* text_to_complete,
					    gchar** remaining_text,
					    CompletionState* cmpl_state);
#ifndef G_PLATFORM_WIN32
static gboolean       check_dir            (gchar *dir_name, 
					    struct stat *result, 
					    gboolean *stat_subdirs);
#endif
static CompletionDir* open_dir             (gchar* dir_name,
					    CompletionState* cmpl_state);
#ifdef HAVE_PWD_H
static CompletionDir* open_user_dir        (const gchar* text_to_complete,
					    CompletionState *cmpl_state);
#endif
static CompletionDir* open_relative_dir    (gchar* dir_name, CompletionDir* dir,
					    CompletionState *cmpl_state);
static CompletionDirSent* open_new_dir     (gchar* dir_name, 
					    struct stat* sbuf,
					    gboolean stat_subdirs);
static gint           correct_dir_fullname (CompletionDir* cmpl_dir);
static gint           correct_parent       (CompletionDir* cmpl_dir,
					    struct stat *sbuf);
#ifndef G_PLATFORM_WIN32
static gchar*         find_parent_dir_fullname    (gchar* dirname);
#endif
static CompletionDir* attach_dir           (CompletionDirSent* sent,
					    gchar* dir_name,
					    CompletionState *cmpl_state);
static void           free_dir_sent (CompletionDirSent* sent);
static void           free_dir      (CompletionDir  *dir);
static void           prune_memory_usage(CompletionState *cmpl_state);

/* Completion operations */
#ifdef HAVE_PWD_H
static PossibleCompletion* attempt_homedir_completion(gchar* text_to_complete,
						      CompletionState *cmpl_state);
#endif
static PossibleCompletion* attempt_file_completion(CompletionState *cmpl_state);
static CompletionDir* find_completion_dir(gchar* text_to_complete,
					  gchar** remaining_text,
					  CompletionState* cmpl_state);
static PossibleCompletion* append_completion_text(gchar* text,
						  CompletionState* cmpl_state);
#ifdef HAVE_PWD_H
static gint get_pwdb(CompletionState* cmpl_state);
static gint compare_user_dir(const void* a, const void* b);
#endif
static gint first_diff_index(gchar* pat, gchar* text);
static gint compare_cmpl_dir(const void* a, const void* b);
static void update_cmpl(PossibleCompletion* poss,
			CompletionState* cmpl_state);

static void gtk_file_selection_set_property  (GObject         *object,
					      guint            prop_id,
					      const GValue    *value,
					      GParamSpec      *pspec);
static void gtk_file_selection_get_property  (GObject         *object,
					      guint            prop_id,
					      GValue          *value,
					      GParamSpec      *pspec);
static void gtk_file_selection_finalize      (GObject               *object);
static void gtk_file_selection_destroy       (GtkObject             *object);
static void gtk_file_selection_map           (GtkWidget             *widget);
static gint gtk_file_selection_key_press     (GtkWidget             *widget,
					      GdkEventKey           *event,
					      gpointer               user_data);
static gint gtk_file_selection_insert_text   (GtkWidget             *widget,
					      const gchar           *new_text,
					      gint                   new_text_length,
					      gint                  *position,
					      gpointer               user_data);
static void gtk_file_selection_update_fileops (GtkFileSelection     *filesel);

static void gtk_file_selection_file_activate (GtkTreeView       *tree_view,
					      GtkTreePath       *path,
					      GtkTreeViewColumn *column,
					      gpointer           user_data);
static void gtk_file_selection_file_changed  (GtkTreeSelection  *selection,
					      gpointer           user_data);
static void gtk_file_selection_dir_activate  (GtkTreeView       *tree_view,
					      GtkTreePath       *path,
					      GtkTreeViewColumn *column,
					      gpointer           user_data);

static void gtk_file_selection_populate      (GtkFileSelection      *fs,
					      gchar                 *rel_path,
					      gboolean               try_complete,
					      gboolean               reset_entry);
static void gtk_file_selection_abort         (GtkFileSelection      *fs);

static void gtk_file_selection_update_history_menu (GtkFileSelection       *fs,
						    gchar                  *current_dir);

static void gtk_file_selection_create_dir  (GtkWidget *widget, gpointer data);
static void gtk_file_selection_delete_file (GtkWidget *widget, gpointer data);
static void gtk_file_selection_rename_file (GtkWidget *widget, gpointer data);

static void free_selected_names (GPtrArray *names);

#ifndef G_PLATFORM_WIN32

#define compare_utf8_filenames(a, b) strcmp(a, b)
#define compare_sys_filenames(a, b) strcmp(a, b)

#else

static gint
compare_utf8_filenames (const gchar *a,
			const gchar *b)
{
  gchar *a_folded, *b_folded;
  gint retval;

  a_folded = g_utf8_strdown (a, -1);
  b_folded = g_utf8_strdown (b, -1);

  retval = strcmp (a_folded, b_folded);

  g_free (a_folded);
  g_free (b_folded);

  return retval;
}

static gint
compare_sys_filenames (const gchar *a,
		       const gchar *b)
{
  gchar *a_utf8, *b_utf8;
  gint retval;

  a_utf8 = g_filename_to_utf8 (a, -1, NULL, NULL, NULL);
  b_utf8 = g_filename_to_utf8 (b, -1, NULL, NULL, NULL);

  retval = compare_utf8_filenames (a_utf8, b_utf8);

  g_free (a_utf8);
  g_free (b_utf8);

  return retval;
}

#endif

/* Saves errno when something cmpl does fails. */
static gint cmpl_errno;

#ifdef G_WITH_CYGWIN
/*
 * Take the path currently in the file selection
 * entry field and translate as necessary from
 * a Win32 style to Cygwin style path.  For
 * instance translate:
 * x:\somepath\file.jpg
 * to:
 * /cygdrive/x/somepath/file.jpg
 *
 * Replace the path in the selection text field.
 * Return a boolean value concerning whether a
 * translation had to be made.
 */
static int
translate_win32_path (GtkFileSelection *filesel)
{
  int updated = 0;
  const gchar *path;
  gchar newPath[MAX_PATH];

  /*
   * Retrieve the current path
   */
  path = gtk_entry_get_text (GTK_ENTRY (filesel->selection_entry));

  cygwin_conv_to_posix_path (path, newPath);
  updated = (strcmp (path, newPath) != 0);

  if (updated)
    gtk_entry_set_text (GTK_ENTRY (filesel->selection_entry), newPath);
    
  return updated;
}
#endif

G_DEFINE_TYPE (GtkFileSelection, gtk_file_selection, GTK_TYPE_DIALOG)

static void
gtk_file_selection_class_init (GtkFileSelectionClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->finalize = gtk_file_selection_finalize;
  gobject_class->set_property = gtk_file_selection_set_property;
  gobject_class->get_property = gtk_file_selection_get_property;
   
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        P_("Filename"),
                                                        P_("The currently selected filename"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_FILEOPS,
				   g_param_spec_boolean ("show-fileops",
							 P_("Show file operations"),
							 P_("Whether buttons for creating/manipulating files should be displayed"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SELECT_MULTIPLE,
				   g_param_spec_boolean ("select-multiple",
							 P_("Select Multiple"),
							 P_("Whether to allow multiple files to be selected"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  object_class->destroy = gtk_file_selection_destroy;
  widget_class->map = gtk_file_selection_map;
}

static void gtk_file_selection_set_property (GObject         *object,
					     guint            prop_id,
					     const GValue    *value,
					     GParamSpec      *pspec)
{
  GtkFileSelection *filesel;

  filesel = GTK_FILE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      gtk_file_selection_set_filename (filesel,
                                       g_value_get_string (value));
      break;
    case PROP_SHOW_FILEOPS:
      if (g_value_get_boolean (value))
	 gtk_file_selection_show_fileop_buttons (filesel);
      else
	 gtk_file_selection_hide_fileop_buttons (filesel);
      break;
    case PROP_SELECT_MULTIPLE:
      gtk_file_selection_set_select_multiple (filesel, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_file_selection_get_property (GObject         *object,
					     guint            prop_id,
					     GValue          *value,
					     GParamSpec      *pspec)
{
  GtkFileSelection *filesel;

  filesel = GTK_FILE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value,
                          gtk_file_selection_get_filename(filesel));
      break;

    case PROP_SHOW_FILEOPS:
      /* This is a little bit hacky, but doing otherwise would require
       * adding a field to the object.
       */
      g_value_set_boolean (value, (filesel->fileop_c_dir && 
				   filesel->fileop_del_file &&
				   filesel->fileop_ren_file));
      break;
    case PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, gtk_file_selection_get_select_multiple (filesel));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
grab_default (GtkWidget *widget)
{
  gtk_widget_grab_default (widget);
  return FALSE;
}
     
static void
gtk_file_selection_init (GtkFileSelection *filesel)
{
  GtkWidget *entry_vbox;
  GtkWidget *label;
  GtkWidget *list_hbox, *list_container;
  GtkWidget *pulldown_hbox;
  GtkWidget *scrolled_win;
  GtkWidget *eventbox;
  GtkWidget *spacer;
  GtkDialog *dialog;

  GtkListStore *model;
  GtkTreeViewColumn *column;
  
  gtk_widget_push_composite_child ();

  dialog = GTK_DIALOG (filesel);

  filesel->cmpl_state = cmpl_init_state ();

  /* The dialog-sized vertical box  */
  filesel->main_vbox = dialog->vbox;
  gtk_container_set_border_width (GTK_CONTAINER (filesel), 10);

  /* The horizontal box containing create, rename etc. buttons */
  filesel->button_area = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (filesel->button_area), GTK_BUTTONBOX_START);
  gtk_box_set_spacing (GTK_BOX (filesel->button_area), 0);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), filesel->button_area, 
		      FALSE, FALSE, 0);
  gtk_widget_show (filesel->button_area);
  
  gtk_file_selection_show_fileop_buttons (filesel);

  /* hbox for pulldown menu */
  pulldown_hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), pulldown_hbox, FALSE, FALSE, 0);
  gtk_widget_show (pulldown_hbox);
  
  /* Pulldown menu */
  filesel->history_pulldown = gtk_option_menu_new ();
  gtk_widget_show (filesel->history_pulldown);
  gtk_box_pack_start (GTK_BOX (pulldown_hbox), filesel->history_pulldown, 
		      FALSE, FALSE, 0);
    
  /*  The horizontal box containing the directory and file listboxes  */

  spacer = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_size_request (spacer, -1, 5);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), spacer, FALSE, FALSE, 0);
  gtk_widget_show (spacer);
  
  list_hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), list_hbox, TRUE, TRUE, 0);
  gtk_widget_show (list_hbox);
  if (WANT_HPANED)
    list_container = g_object_new (GTK_TYPE_HPANED,
				   "visible", TRUE,
				   "parent", list_hbox,
				   "border_width", 0,
				   NULL);
  else
    list_container = list_hbox;

  spacer = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_size_request (spacer, -1, 5);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), spacer, FALSE, FALSE, 0);  
  gtk_widget_show (spacer);
  
  /* The directories list */

  model = gtk_list_store_new (1, G_TYPE_STRING);
  filesel->dir_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);

  column = gtk_tree_view_column_new_with_attributes (_("Folders"),
						     gtk_cell_renderer_text_new (),
						     "text", DIR_COLUMN,
						     NULL);
  label = gtk_label_new_with_mnemonic (_("Fol_ders"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), filesel->dir_list);
  gtk_widget_show (label);
  gtk_tree_view_column_set_widget (column, label);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (filesel->dir_list), column);

  gtk_widget_set_size_request (filesel->dir_list,
			       DIR_LIST_WIDTH, DIR_LIST_HEIGHT);
  g_signal_connect (filesel->dir_list, "row-activated",
		    G_CALLBACK (gtk_file_selection_dir_activate), filesel);

  /*  gtk_clist_column_titles_passive (GTK_CLIST (filesel->dir_list)); */

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_IN);  
  gtk_container_add (GTK_CONTAINER (scrolled_win), filesel->dir_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 0);
  if (GTK_IS_PANED (list_container))
    gtk_paned_pack1 (GTK_PANED (list_container), scrolled_win, TRUE, TRUE);
  else
    gtk_container_add (GTK_CONTAINER (list_container), scrolled_win);
  gtk_widget_show (filesel->dir_list);
  gtk_widget_show (scrolled_win);

  /* The files list */
  model = gtk_list_store_new (1, G_TYPE_STRING);
  filesel->file_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);

  column = gtk_tree_view_column_new_with_attributes (_("Files"),
						     gtk_cell_renderer_text_new (),
						     "text", FILE_COLUMN,
						     NULL);
  label = gtk_label_new_with_mnemonic (_("_Files"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), filesel->file_list);
  gtk_widget_show (label);
  gtk_tree_view_column_set_widget (column, label);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (filesel->file_list), column);

  gtk_widget_set_size_request (filesel->file_list,
			       FILE_LIST_WIDTH, FILE_LIST_HEIGHT);
  g_signal_connect (filesel->file_list, "row-activated",
		    G_CALLBACK (gtk_file_selection_file_activate), filesel);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (filesel->file_list)), "changed",
		    G_CALLBACK (gtk_file_selection_file_changed), filesel);

  /* gtk_clist_column_titles_passive (GTK_CLIST (filesel->file_list)); */

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (scrolled_win), filesel->file_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 0);
  gtk_container_add (GTK_CONTAINER (list_container), scrolled_win);
  gtk_widget_show (filesel->file_list);
  gtk_widget_show (scrolled_win);

  /* action area for packing buttons into. */
  filesel->action_area = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), filesel->action_area, 
		      FALSE, FALSE, 0);
  gtk_widget_show (filesel->action_area);
  
  /*  The OK/Cancel button area */

  /*  The Cancel button  */
  filesel->cancel_button = gtk_dialog_add_button (dialog,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL);
  /*  The OK button  */
  filesel->ok_button = gtk_dialog_add_button (dialog,
                                              GTK_STOCK_OK,
                                              GTK_RESPONSE_OK);

  gtk_dialog_set_alternative_button_order (dialog,
					   GTK_RESPONSE_OK,
					   GTK_RESPONSE_CANCEL,
					   -1);

  gtk_widget_grab_default (filesel->ok_button);

  /*  The selection entry widget  */
  entry_vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_end (GTK_BOX (filesel->main_vbox), entry_vbox, FALSE, FALSE, 2);
  gtk_widget_show (entry_vbox);
  
  eventbox = gtk_event_box_new ();
  filesel->selection_text = label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (eventbox), label);
  gtk_box_pack_start (GTK_BOX (entry_vbox), eventbox, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (eventbox);

  filesel->selection_entry = gtk_entry_new ();
  g_signal_connect (filesel->selection_entry, "key-press-event",
		    G_CALLBACK (gtk_file_selection_key_press), filesel);
  g_signal_connect (filesel->selection_entry, "insert-text",
		    G_CALLBACK (gtk_file_selection_insert_text), NULL);
  g_signal_connect_swapped (filesel->selection_entry, "changed",
			    G_CALLBACK (gtk_file_selection_update_fileops), filesel);
  g_signal_connect_swapped (filesel->selection_entry, "focus-in-event",
			    G_CALLBACK (grab_default),
			    filesel->ok_button);
  g_signal_connect_swapped (filesel->selection_entry, "activate",
			    G_CALLBACK (gtk_button_clicked),
			    filesel->ok_button);
  
  gtk_box_pack_start (GTK_BOX (entry_vbox), filesel->selection_entry, TRUE, TRUE, 0);
  gtk_widget_show (filesel->selection_entry);

  gtk_label_set_mnemonic_widget (GTK_LABEL (filesel->selection_text),
				 filesel->selection_entry);

  if (!cmpl_state_okay (filesel->cmpl_state))
    {
      gchar err_buf[256];

      g_snprintf (err_buf, sizeof (err_buf), _("Folder unreadable: %s"), cmpl_strerror (cmpl_errno));

      gtk_label_set_text (GTK_LABEL (filesel->selection_text), err_buf);
    }
  else
    {
      gtk_file_selection_populate (filesel, "", FALSE, TRUE);
    }

  gtk_widget_grab_focus (filesel->selection_entry);

  gtk_widget_pop_composite_child ();
}

static void
dnd_really_drop  (GtkWidget *dialog, gint response_id, GtkFileSelection *fs)
{
  gchar *filename;
  
  if (response_id == GTK_RESPONSE_YES)
    {
      filename = g_object_get_data (G_OBJECT (dialog), "gtk-fs-dnd-filename");

      gtk_file_selection_set_filename (fs, filename);
    }
  
  gtk_widget_destroy (dialog);
}

static void
filenames_dropped (GtkWidget        *widget,
		   GdkDragContext   *context,
		   gint              x,
		   gint              y,
		   GtkSelectionData *selection_data,
		   guint             info,
		   guint             time)
{
  char **uris = NULL;
  char *filename = NULL;
  char *hostname;
  const char *this_hostname;
  GError *error = NULL;

  uris = gtk_selection_data_get_uris (selection_data);
  if (!uris || !uris[0])
    {
      g_strfreev (uris);
      return;
    }

  filename = g_filename_from_uri (uris[0], &hostname, &error);
  g_strfreev (uris);
  
  if (!filename)
    {
      g_warning ("Error getting dropped filename: %s\n",
		 error->message);
      g_error_free (error);
      return;
    }

  this_hostname = g_get_host_name ();
  
  if ((hostname == NULL) ||
      (strcmp (hostname, this_hostname) == 0) ||
      (strcmp (hostname, "localhost") == 0))
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (widget),
				     filename);
  else
    {
      GtkWidget *dialog;
      gchar *filename_utf8;

      /* Conversion back to UTF-8 should always succeed for the result
       * of g_filename_from_uri()
       */
      filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_assert (filename_utf8);
      
      dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_YES_NO,
				       _("The file \"%s\" resides on another machine (called %s) and may not be available to this program.\n"
					 "Are you sure that you want to select it?"), filename_utf8, hostname);
      g_free (filename_utf8);

      g_object_set_data_full (G_OBJECT (dialog), I_("gtk-fs-dnd-filename"), g_strdup (filename), g_free);
      
      g_signal_connect_data (dialog, "response",
			     (GCallback) dnd_really_drop, 
			     widget, NULL, 0);
      
      gtk_widget_show (dialog);
    }

  g_free (hostname);
  g_free (filename);
}

static void
filenames_drag_get (GtkWidget        *widget,
		    GdkDragContext   *context,
		    GtkSelectionData *selection_data,
		    guint             info,
		    guint             time,
		    GtkFileSelection *filesel)
{
  const gchar *file;
  gchar *filename_utf8;

  file = gtk_file_selection_get_filename (filesel);
  if (!file)
    return;

  if (gtk_targets_include_uri (&selection_data->target, 1))
    {
      gchar *file_uri;
      const char *hostname;
      GError *error;
      char *uris[2];

      hostname = g_get_host_name ();

      error = NULL;
      file_uri = g_filename_to_uri (file, hostname, &error);
      if (!file_uri)
        {
          g_warning ("Error getting filename: %s\n",
                      error->message);
          g_error_free (error);
          return;
        }
	  
      uris[0] = file_uri;
      uris[1] = NULL;
      gtk_selection_data_set_uris (selection_data, uris);
      g_free (file_uri);

      return;
    }
	  
  filename_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);
  if (!filename_utf8)
    return;

  gtk_selection_data_set_text (selection_data, filename_utf8, -1);
  g_free (filename_utf8);
}

static void
file_selection_setup_dnd (GtkFileSelection *filesel)
{
  GtkWidget *eventbox;

  gtk_drag_dest_set (GTK_WIDGET (filesel),
		     GTK_DEST_DEFAULT_ALL,
		     NULL, 0,
		     GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets (GTK_WIDGET (filesel));

  g_signal_connect (filesel, "drag-data-received",
		    G_CALLBACK (filenames_dropped), NULL);

  eventbox = gtk_widget_get_parent (filesel->selection_text);
  gtk_drag_source_set (eventbox,
		       GDK_BUTTON1_MASK,
		       NULL, 0,
		       GDK_ACTION_COPY);
  gtk_drag_source_add_uri_targets (eventbox);
  gtk_drag_source_add_text_targets (eventbox);

  g_signal_connect (eventbox, "drag-data-get",
		    G_CALLBACK (filenames_drag_get), filesel);
}

GtkWidget*
gtk_file_selection_new (const gchar *title)
{
  GtkFileSelection *filesel;

  filesel = g_object_new (GTK_TYPE_FILE_SELECTION, NULL);
  gtk_window_set_title (GTK_WINDOW (filesel), title);
  gtk_dialog_set_has_separator (GTK_DIALOG (filesel), FALSE);

  file_selection_setup_dnd (filesel);
  
  return GTK_WIDGET (filesel);
}

void
gtk_file_selection_show_fileop_buttons (GtkFileSelection *filesel)
{
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
    
  /* delete, create directory, and rename */
  if (!filesel->fileop_c_dir) 
    {
      filesel->fileop_c_dir = gtk_button_new_with_mnemonic (_("_New Folder"));
      g_signal_connect (filesel->fileop_c_dir, "clicked",
			G_CALLBACK (gtk_file_selection_create_dir),
			filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_c_dir, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_c_dir);
    }
	
  if (!filesel->fileop_del_file) 
    {
      filesel->fileop_del_file = gtk_button_new_with_mnemonic (_("De_lete File"));
      g_signal_connect (filesel->fileop_del_file, "clicked",
			G_CALLBACK (gtk_file_selection_delete_file),
			filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_del_file, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_del_file);
    }

  if (!filesel->fileop_ren_file)
    {
      filesel->fileop_ren_file = gtk_button_new_with_mnemonic (_("_Rename File"));
      g_signal_connect (filesel->fileop_ren_file, "clicked",
			G_CALLBACK (gtk_file_selection_rename_file),
			filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_ren_file, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_ren_file);
    }
  
  gtk_file_selection_update_fileops (filesel);
  
  g_object_notify (G_OBJECT (filesel), "show-fileops");
}

void       
gtk_file_selection_hide_fileop_buttons (GtkFileSelection *filesel)
{
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
    
  if (filesel->fileop_ren_file)
    {
      gtk_widget_destroy (filesel->fileop_ren_file);
      filesel->fileop_ren_file = NULL;
    }

  if (filesel->fileop_del_file)
    {
      gtk_widget_destroy (filesel->fileop_del_file);
      filesel->fileop_del_file = NULL;
    }

  if (filesel->fileop_c_dir)
    {
      gtk_widget_destroy (filesel->fileop_c_dir);
      filesel->fileop_c_dir = NULL;
    }
  g_object_notify (G_OBJECT (filesel), "show-fileops");
}



/**
 * gtk_file_selection_set_filename:
 * @filesel: a #GtkFileSelection.
 * @filename:  a string to set as the default file name.
 * 
 * Sets a default path for the file requestor. If @filename includes a
 * directory path, then the requestor will open with that path as its
 * current working directory.
 *
 * This has the consequence that in order to open the requestor with a 
 * working directory and an empty filename, @filename must have a trailing
 * directory separator.
 *
 * The encoding of @filename is preferred GLib file name encoding, which
 * may not be UTF-8. See g_filename_from_utf8().
 **/
void
gtk_file_selection_set_filename (GtkFileSelection *filesel,
				 const gchar      *filename)
{
  gchar *buf;
  const char *name, *last_slash;
  char *filename_utf8;

  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
  g_return_if_fail (filename != NULL);

  filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  g_return_if_fail (filename_utf8 != NULL);

  last_slash = strrchr (filename_utf8, G_DIR_SEPARATOR);

  if (!last_slash)
    {
      buf = g_strdup ("");
      name = filename_utf8;
    }
  else
    {
      buf = g_strdup (filename_utf8);
      buf[last_slash - filename_utf8 + 1] = 0;
      name = last_slash + 1;
    }

  gtk_file_selection_populate (filesel, buf, FALSE, TRUE);

  if (filesel->selection_entry)
    gtk_entry_set_text (GTK_ENTRY (filesel->selection_entry), name);
  g_free (buf);
  g_object_notify (G_OBJECT (filesel), "filename");

  g_free (filename_utf8);
}

/**
 * gtk_file_selection_get_filename:
 * @filesel: a #GtkFileSelection
 * 
 * This function returns the selected filename in the GLib file name
 * encoding. To convert to UTF-8, call g_filename_to_utf8(). The
 * returned string points to a statically allocated buffer and should
 * be copied if you plan to keep it around.
 *
 * If no file is selected then the selected directory path is returned.
 * 
 * Return value: currently-selected filename in the on-disk encoding.
 **/
const gchar*
gtk_file_selection_get_filename (GtkFileSelection *filesel)
{
  static const gchar nothing[2] = "";
  static GString *something;
  char *sys_filename;
  const char *text;

  g_return_val_if_fail (GTK_IS_FILE_SELECTION (filesel), nothing);

#ifdef G_WITH_CYGWIN
  translate_win32_path (filesel);
#endif
  text = gtk_entry_get_text (GTK_ENTRY (filesel->selection_entry));
  if (text)
    {
      gchar *fullname = cmpl_completion_fullname (text, filesel->cmpl_state);
      sys_filename = g_filename_from_utf8 (fullname, -1, NULL, NULL, NULL);
      g_free (fullname);
      if (!sys_filename)
	return nothing;
      if (!something)
        something = g_string_new (sys_filename);
      else
        g_string_assign (something, sys_filename);
      g_free (sys_filename);

      return something->str;
    }

  return nothing;
}

void
gtk_file_selection_complete (GtkFileSelection *filesel,
			     const gchar      *pattern)
{
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
  g_return_if_fail (pattern != NULL);

  if (filesel->selection_entry)
    gtk_entry_set_text (GTK_ENTRY (filesel->selection_entry), pattern);
  gtk_file_selection_populate (filesel, (gchar*) pattern, TRUE, TRUE);
}

static void
gtk_file_selection_destroy (GtkObject *object)
{
  GtkFileSelection *filesel;
  GList *list;
  HistoryCallbackArg *callback_arg;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (object));
  
  filesel = GTK_FILE_SELECTION (object);
  
  if (filesel->fileop_dialog)
    {
      gtk_widget_destroy (filesel->fileop_dialog);
      filesel->fileop_dialog = NULL;
    }
  
  if (filesel->history_list)
    {
      list = filesel->history_list;
      while (list)
	{
	  callback_arg = list->data;
	  g_free (callback_arg->directory);
	  g_free (callback_arg);
	  list = list->next;
	}
      g_list_free (filesel->history_list);
      filesel->history_list = NULL;
    }

  if (filesel->cmpl_state)
    {
      cmpl_free_state (filesel->cmpl_state);
      filesel->cmpl_state = NULL;
    }
 
  if (filesel->selected_names)
    {
      free_selected_names (filesel->selected_names);
      filesel->selected_names = NULL;
    } 

  if (filesel->last_selected)
    {
      g_free (filesel->last_selected);
      filesel->last_selected = NULL;
    }

  GTK_OBJECT_CLASS (gtk_file_selection_parent_class)->destroy (object);
}

static void
gtk_file_selection_map (GtkWidget *widget)
{
  GtkFileSelection *filesel = GTK_FILE_SELECTION (widget);

  /* Refresh the contents */
  gtk_file_selection_populate (filesel, "", FALSE, FALSE);
  
  GTK_WIDGET_CLASS (gtk_file_selection_parent_class)->map (widget);
}

static void
gtk_file_selection_finalize (GObject *object)
{
  GtkFileSelection *filesel = GTK_FILE_SELECTION (object);

  g_free (filesel->fileop_file);

  G_OBJECT_CLASS (gtk_file_selection_parent_class)->finalize (object);
}

/* Begin file operations callbacks */

static void
gtk_file_selection_fileop_error (GtkFileSelection *fs,
				 gchar            *error_message)
{
  GtkWidget *dialog;
    
  g_return_if_fail (error_message != NULL);

  /* main dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (fs),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   "%s", error_message);

  /* yes, we free it */
  g_free (error_message);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
 
  g_signal_connect_swapped (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy),
			    dialog);

  gtk_widget_show (dialog);
}

static void
gtk_file_selection_fileop_destroy (GtkWidget *widget,
				   gpointer   data)
{
  GtkFileSelection *fs = data;

  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));
  
  fs->fileop_dialog = NULL;
}

static gboolean
entry_is_empty (GtkEntry *entry)
{
  const gchar *text = gtk_entry_get_text (entry);
  
  return *text == '\0';
}

static void
gtk_file_selection_fileop_entry_changed (GtkEntry   *entry,
					 GtkWidget  *button)
{
  gtk_widget_set_sensitive (button, !entry_is_empty (entry));
}

static void
gtk_file_selection_create_dir_confirmed (GtkWidget *widget,
					 gpointer   data)
{
  GtkFileSelection *fs = data;
  const gchar *dirname;
  gchar *path;
  gchar *full_path;
  gchar *sys_full_path;
  gchar *buf;
  GError *error = NULL;
  CompletionState *cmpl_state;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  dirname = gtk_entry_get_text (GTK_ENTRY (fs->fileop_entry));
  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  full_path = g_strconcat (path, G_DIR_SEPARATOR_S, dirname, NULL);
  sys_full_path = g_filename_from_utf8 (full_path, -1, NULL, NULL, &error);
  if (error)
    {
      if (g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
	buf = g_strdup_printf (_("The folder name \"%s\" contains symbols that are not allowed in filenames"), dirname);
      else
	buf = g_strdup_printf (_("Error creating folder '%s': %s"), 
			       dirname, error->message);
      gtk_file_selection_fileop_error (fs, buf);
      g_error_free (error);
      goto out;
    }

  if (g_mkdir (sys_full_path, 0777) < 0)
    {
      int errsv = errno;

      buf = g_strdup_printf (_("Error creating folder '%s': %s"), 
			     dirname, g_strerror (errsv));
      gtk_file_selection_fileop_error (fs, buf);
    }

 out:
  g_free (full_path);
  g_free (sys_full_path);
  
  gtk_widget_destroy (fs->fileop_dialog);
  gtk_file_selection_populate (fs, "", FALSE, FALSE);
}
  
static void
gtk_file_selection_create_dir (GtkWidget *widget,
			       gpointer   data)
{
  GtkFileSelection *fs = data;
  GtkWidget *label;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *button;

  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
    return;
  
  /* main dialog */
  dialog = gtk_dialog_new ();
  fs->fileop_dialog = dialog;
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_file_selection_fileop_destroy),
		    fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("New Folder"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (fs));

  /* If file dialog is grabbed, grab option dialog */
  /* When option dialog is closed, file dialog will be grabbed again */
  if (GTK_WINDOW (fs)->modal)
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
		     FALSE, FALSE, 0);
  gtk_widget_show( vbox);
  
  label = gtk_label_new_with_mnemonic (_("_Folder name:"));
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  /*  The directory entry widget  */
  fs->fileop_entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), fs->fileop_entry);
  gtk_box_pack_start (GTK_BOX (vbox), fs->fileop_entry, 
		      TRUE, TRUE, 5);
  gtk_widget_set_can_default (fs->fileop_entry, TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (fs->fileop_entry), TRUE); 
  gtk_widget_show (fs->fileop_entry);
  
  /* buttons */
  button = gtk_dialog_add_button (GTK_DIALOG (dialog), 
				  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    dialog);

  gtk_widget_grab_focus (fs->fileop_entry);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog), 
				  _("C_reate"), GTK_RESPONSE_OK);
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (gtk_file_selection_create_dir_confirmed),
		    fs);
  g_signal_connect (fs->fileop_entry, "changed",
                    G_CALLBACK (gtk_file_selection_fileop_entry_changed),
		    button);

  gtk_widget_grab_default (button);
  
  gtk_widget_show (dialog);
}

static void
gtk_file_selection_delete_file_response (GtkDialog *dialog, 
                                         gint       response_id,
                                         gpointer   data)
{
  GtkFileSelection *fs = data;
  CompletionState *cmpl_state;
  gchar *path;
  gchar *full_path;
  gchar *sys_full_path;
  GError *error = NULL;
  gchar *buf;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (response_id != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  full_path = g_strconcat (path, G_DIR_SEPARATOR_S, fs->fileop_file, NULL);
  sys_full_path = g_filename_from_utf8 (full_path, -1, NULL, NULL, &error);
  if (error)
    {
      if (g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
	buf = g_strdup_printf (_("The filename \"%s\" contains symbols that are not allowed in filenames"),
			       fs->fileop_file);
      else
	buf = g_strdup_printf (_("Error deleting file '%s': %s"),
			       fs->fileop_file, error->message);
      
      gtk_file_selection_fileop_error (fs, buf);
      g_error_free (error);
      goto out;
    }

  if (g_unlink (sys_full_path) < 0) 
    {
      int errsv = errno;

      buf = g_strdup_printf (_("Error deleting file '%s': %s"),
			     fs->fileop_file, g_strerror (errsv));
      gtk_file_selection_fileop_error (fs, buf);
    }
  
 out:
  g_free (full_path);
  g_free (sys_full_path);
  
  gtk_widget_destroy (fs->fileop_dialog);
  gtk_file_selection_populate (fs, "", FALSE, TRUE);
}

static void
gtk_file_selection_delete_file (GtkWidget *widget,
				gpointer   data)
{
  GtkFileSelection *fs = data;
  GtkWidget *dialog;
  const gchar *filename;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
    return;

#ifdef G_WITH_CYGWIN
  translate_win32_path (fs);
#endif

  filename = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));
  if (strlen (filename) < 1)
    return;

  g_free (fs->fileop_file);
  fs->fileop_file = g_strdup (filename);
  
  /* main dialog */
  fs->fileop_dialog = dialog = 
    gtk_message_dialog_new (GTK_WINDOW (fs),
                            GTK_WINDOW (fs)->modal ? GTK_DIALOG_MODAL : 0,
                            GTK_MESSAGE_QUESTION,
                            GTK_BUTTONS_NONE,
                            _("Really delete file \"%s\"?"), filename);

  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_file_selection_fileop_destroy),
		    fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete File"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  
  /* buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_DELETE, GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_file_selection_delete_file_response),
                    fs);
  
  gtk_widget_show (dialog);
}

static void
gtk_file_selection_rename_file_confirmed (GtkWidget *widget,
					  gpointer   data)
{
  GtkFileSelection *fs = data;
  gchar *buf;
  const gchar *file;
  gchar *path;
  gchar *new_filename;
  gchar *old_filename;
  gchar *sys_new_filename;
  gchar *sys_old_filename;
  CompletionState *cmpl_state;
  GError *error = NULL;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  file = gtk_entry_get_text (GTK_ENTRY (fs->fileop_entry));
  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  new_filename = g_strconcat (path, G_DIR_SEPARATOR_S, file, NULL);
  old_filename = g_strconcat (path, G_DIR_SEPARATOR_S, fs->fileop_file, NULL);

  sys_new_filename = g_filename_from_utf8 (new_filename, -1, NULL, NULL, &error);
  if (error)
    {
      if (g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
	buf = g_strdup_printf (_("The filename \"%s\" contains symbols that are not allowed in filenames"), new_filename);
      else
	buf = g_strdup_printf (_("Error renaming file to \"%s\": %s"),
			       new_filename, error->message);
      gtk_file_selection_fileop_error (fs, buf);
      g_error_free (error);
      goto out1;
    }

  sys_old_filename = g_filename_from_utf8 (old_filename, -1, NULL, NULL, &error);
  if (error)
    {
      if (g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
	buf = g_strdup_printf (_("The filename \"%s\" contains symbols that are not allowed in filenames"), old_filename);
      else
	buf = g_strdup_printf (_("Error renaming file \"%s\": %s"),
			       old_filename, error->message);
      gtk_file_selection_fileop_error (fs, buf);
      g_error_free (error);
      goto out2;
    }
  
  if (g_rename (sys_old_filename, sys_new_filename) < 0) 
    {
      int errsv = errno;

      buf = g_strdup_printf (_("Error renaming file \"%s\" to \"%s\": %s"),
			     sys_old_filename, sys_new_filename,
			     g_strerror (errsv));
      gtk_file_selection_fileop_error (fs, buf);
      goto out2;
    }
  
  gtk_file_selection_populate (fs, "", FALSE, FALSE);
  gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), file);
  
 out2:
  g_free (sys_old_filename);

 out1:
  g_free (new_filename);
  g_free (old_filename);
  g_free (sys_new_filename);
  
  gtk_widget_destroy (fs->fileop_dialog);
}
  
static void
gtk_file_selection_rename_file (GtkWidget *widget,
				gpointer   data)
{
  GtkFileSelection *fs = data;
  GtkWidget *label;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *button;
  gchar *buf;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
	  return;

  g_free (fs->fileop_file);
  fs->fileop_file = g_strdup (gtk_entry_get_text (GTK_ENTRY (fs->selection_entry)));
  if (strlen (fs->fileop_file) < 1)
    return;
  
  /* main dialog */
  fs->fileop_dialog = dialog = gtk_dialog_new ();
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_file_selection_fileop_destroy),
		    fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Rename File"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (fs));

  /* If file dialog is grabbed, grab option dialog */
  /* When option dialog  closed, file dialog will be grabbed again */
  if (GTK_WINDOW (fs)->modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
		      FALSE, FALSE, 0);
  gtk_widget_show(vbox);
  
  buf = g_strdup_printf (_("Rename file \"%s\" to:"), fs->fileop_file);
  label = gtk_label_new (buf);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
  gtk_widget_show (label);
  g_free (buf);

  /* New filename entry */
  fs->fileop_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), fs->fileop_entry, 
		      TRUE, TRUE, 5);
  gtk_widget_set_can_default (fs->fileop_entry, TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (fs->fileop_entry), TRUE); 
  gtk_widget_show (fs->fileop_entry);
  
  gtk_entry_set_text (GTK_ENTRY (fs->fileop_entry), fs->fileop_file);
  gtk_editable_select_region (GTK_EDITABLE (fs->fileop_entry),
			      0, strlen (fs->fileop_file));

  /* buttons */
  button = gtk_dialog_add_button (GTK_DIALOG (dialog), 
				  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    dialog);

  gtk_widget_grab_focus (fs->fileop_entry);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog), 
				  _("_Rename"), GTK_RESPONSE_OK);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (gtk_file_selection_rename_file_confirmed),
		    fs);
  g_signal_connect (fs->fileop_entry, "changed",
		    G_CALLBACK (gtk_file_selection_fileop_entry_changed),
		    button);

  gtk_widget_grab_default (button);
  
  gtk_widget_show (dialog);
}

static gint
gtk_file_selection_insert_text (GtkWidget   *widget,
				const gchar *new_text,
				gint         new_text_length,
				gint        *position,
				gpointer     user_data)
{
  gchar *filename;

  filename = g_filename_from_utf8 (new_text, new_text_length, NULL, NULL, NULL);

  if (!filename)
    {
      gdk_display_beep (gtk_widget_get_display (widget));
      g_signal_stop_emission_by_name (widget, "insert-text");
      return FALSE;
    }
  
  g_free (filename);
  
  return TRUE;
}

static void
gtk_file_selection_update_fileops (GtkFileSelection *fs)
{
  gboolean sensitive;

  if (!fs->selection_entry)
    return;

  sensitive = !entry_is_empty (GTK_ENTRY (fs->selection_entry));

  if (fs->fileop_del_file)
    gtk_widget_set_sensitive (fs->fileop_del_file, sensitive);
  
  if (fs->fileop_ren_file)
    gtk_widget_set_sensitive (fs->fileop_ren_file, sensitive);
}

static gint
gtk_file_selection_key_press (GtkWidget   *widget,
			      GdkEventKey *event,
			      gpointer     user_data)
{
  GtkFileSelection *fs;
  char *text;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if ((event->keyval == GDK_Tab || event->keyval == GDK_KP_Tab) &&
      (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
    {
      fs = GTK_FILE_SELECTION (user_data);
#ifdef G_WITH_CYGWIN
      translate_win32_path (fs);
#endif
      text = g_strdup (gtk_entry_get_text (GTK_ENTRY (fs->selection_entry)));

      gtk_file_selection_populate (fs, text, TRUE, TRUE);

      g_free (text);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_file_selection_history_callback (GtkWidget *widget,
				     gpointer   data)
{
  GtkFileSelection *fs = data;
  HistoryCallbackArg *callback_arg;
  GList *list;

  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  list = fs->history_list;
  
  while (list) {
    callback_arg = list->data;
    
    if (callback_arg->menu_item == widget)
      {
	gtk_file_selection_populate (fs, callback_arg->directory, FALSE, FALSE);
	break;
      }
    
    list = list->next;
  }
}

static void 
gtk_file_selection_update_history_menu (GtkFileSelection *fs,
					gchar            *current_directory)
{
  HistoryCallbackArg *callback_arg;
  GtkWidget *menu_item;
  GList *list;
  gchar *current_dir;
  gint dir_len;
  gint i;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));
  g_return_if_fail (current_directory != NULL);
  
  list = fs->history_list;

  if (fs->history_menu) 
    {
      while (list) {
	callback_arg = list->data;
	g_free (callback_arg->directory);
	g_free (callback_arg);
	list = list->next;
      }
      g_list_free (fs->history_list);
      fs->history_list = NULL;
      
      gtk_widget_destroy (fs->history_menu);
    }
  
  fs->history_menu = gtk_menu_new ();

  current_dir = g_strdup (current_directory);

  dir_len = strlen (current_dir);

  for (i = dir_len; i >= 0; i--)
    {
      /* the i == dir_len is to catch the full path for the first 
       * entry. */
      if ( (current_dir[i] == G_DIR_SEPARATOR) || (i == dir_len))
	{
	  /* another small hack to catch the full path */
	  if (i != dir_len) 
		  current_dir[i + 1] = '\0';
#ifdef G_WITH_CYGWIN
	  if (!strcmp (current_dir, "//"))
	    continue;
#endif
	  menu_item = gtk_menu_item_new_with_label (current_dir);
	  
	  callback_arg = g_new (HistoryCallbackArg, 1);
	  callback_arg->menu_item = menu_item;
	  
	  /* since the autocompletion gets confused if you don't 
	   * supply a trailing '/' on a dir entry, set the full
	   * (current) path to "" which just refreshes the filesel */
	  if (dir_len == i)
	    {
	      callback_arg->directory = g_strdup ("");
	    }
	  else
	    {
	      callback_arg->directory = g_strdup (current_dir);
	    }
	  
	  fs->history_list = g_list_append (fs->history_list, callback_arg);
	  
	  g_signal_connect (menu_item, "activate",
			    G_CALLBACK (gtk_file_selection_history_callback),
			    fs);
	  gtk_menu_shell_append (GTK_MENU_SHELL (fs->history_menu), menu_item);
	  gtk_widget_show (menu_item);
	}
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (fs->history_pulldown), 
			    fs->history_menu);
  g_free (current_dir);
}

static gchar *
get_real_filename (gchar    *filename,
                   gboolean  free_old)
{
#ifdef G_WITH_CYGWIN
  /* Check to see if the selection was a drive selector */
  if (isalpha (filename[0]) && (filename[1] == ':'))
    {
      gchar temp_filename[MAX_PATH];
      int len;

      cygwin_conv_to_posix_path (filename, temp_filename);

      /* we need trailing '/'. */
      len = strlen (temp_filename);
      if (len > 0 && temp_filename[len-1] != '/')
        {
          temp_filename[len]   = '/';
          temp_filename[len+1] = '\0';
        }
      
      if (free_old)
	g_free (filename);

      return g_strdup (temp_filename);
    }
#endif /* G_WITH_CYGWIN */
  return filename;
}

static void
gtk_file_selection_file_activate (GtkTreeView       *tree_view,
				  GtkTreePath       *path,
				  GtkTreeViewColumn *column,
				  gpointer           user_data)
{
  GtkFileSelection *fs = GTK_FILE_SELECTION (user_data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter iter;  
  gchar *filename;
  
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, FILE_COLUMN, &filename, -1);
  filename = get_real_filename (filename, TRUE);
  gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);
  gtk_button_clicked (GTK_BUTTON (fs->ok_button));

  g_free (filename);
}

static void
gtk_file_selection_dir_activate (GtkTreeView       *tree_view,
				 GtkTreePath       *path,
				 GtkTreeViewColumn *column,
				 gpointer           user_data)
{
  GtkFileSelection *fs = GTK_FILE_SELECTION (user_data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter iter;
  gchar *filename;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, DIR_COLUMN, &filename, -1);
  filename = get_real_filename (filename, TRUE);
  gtk_file_selection_populate (fs, filename, FALSE, FALSE);
  g_free (filename);
}

#ifdef G_PLATFORM_WIN32

static void
win32_gtk_add_drives_to_dir_list (GtkListStore *model)
{
  gchar *textPtr;
  gchar buffer[128];
  char formatBuffer[128];
  GtkTreeIter iter;

  /* Get the drives string */
  GetLogicalDriveStrings (sizeof (buffer), buffer);

  /* Add the drives as necessary */
  textPtr = buffer;
  while (*textPtr != '\0')
    {
      /* Ignore floppies (?) */
      if (GetDriveType (textPtr) != DRIVE_REMOVABLE)
	{
	  /* Build the actual displayable string */
	  g_snprintf (formatBuffer, sizeof (formatBuffer), "%c:\\", toupper (textPtr[0]));

	  /* Add to the list */
	  gtk_list_store_append (model, &iter);
	  gtk_list_store_set (model, &iter, DIR_COLUMN, formatBuffer, -1);
	}
      textPtr += (strlen (textPtr) + 1);
    }
}
#endif

static gchar *
escape_underscores (const gchar *str)
{
  GString *result = g_string_new (NULL);
  while (*str)
    {
      if (*str == '_')
	g_string_append_c (result, '_');

      g_string_append_c (result, *str);
      str++;
    }

  return g_string_free (result, FALSE);
}

static void
gtk_file_selection_populate (GtkFileSelection *fs,
			     gchar            *rel_path,
			     gboolean          try_complete,
			     gboolean          reset_entry)
{
  CompletionState *cmpl_state;
  PossibleCompletion* poss;
  GtkTreeIter iter;
  GtkListStore *dir_model;
  GtkListStore *file_model;
  gchar* filename;
  gchar* rem_path = rel_path;
  gchar* sel_text;
  gint did_recurse = FALSE;
  gint possible_count = 0;
  
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  cmpl_state = (CompletionState*) fs->cmpl_state;
  poss = cmpl_completion_matches (rel_path, &rem_path, cmpl_state);

  if (!cmpl_state_okay (cmpl_state))
    {
      /* Something went wrong. */
      gtk_file_selection_abort (fs);
      return;
    }

  g_assert (cmpl_state->reference_dir);

  dir_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (fs->dir_list)));
  file_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (fs->file_list)));

  gtk_list_store_clear (dir_model);
  gtk_list_store_clear (file_model);

  /* Set the dir list to include ./ and ../ */
  gtk_list_store_append (dir_model, &iter);
  gtk_list_store_set (dir_model, &iter, DIR_COLUMN, "." G_DIR_SEPARATOR_S, -1);
  gtk_list_store_append (dir_model, &iter);
  gtk_list_store_set (dir_model, &iter, DIR_COLUMN, ".." G_DIR_SEPARATOR_S, -1);

  while (poss)
    {
      if (cmpl_is_a_completion (poss))
        {
          possible_count += 1;

          filename = cmpl_this_completion (poss);

          if (cmpl_is_directory (poss))
            {
              if (strcmp (filename, "." G_DIR_SEPARATOR_S) != 0 &&
                  strcmp (filename, ".." G_DIR_SEPARATOR_S) != 0)
		{
		  gtk_list_store_append (dir_model, &iter);
		  gtk_list_store_set (dir_model, &iter, DIR_COLUMN, filename, -1);
		}
	    }
          else
	    {
	      gtk_list_store_append (file_model, &iter);
	      gtk_list_store_set (file_model, &iter, DIR_COLUMN, filename, -1);
            }
	}

      poss = cmpl_next_completion (cmpl_state);
    }

#ifdef G_PLATFORM_WIN32
  /* For Windows, add drives as potential selections */
  win32_gtk_add_drives_to_dir_list (dir_model);
#endif

  /* File lists are set. */

  g_assert (cmpl_state->reference_dir);

  if (try_complete)
    {

      /* User is trying to complete filenames, so advance the user's input
       * string to the updated_text, which is the common leading substring
       * of all possible completions, and if its a directory attempt
       * attempt completions in it. */

      if (cmpl_updated_text (cmpl_state)[0])
        {

          if (cmpl_updated_dir (cmpl_state))
            {
	      gchar* dir_name = g_strdup (cmpl_updated_text (cmpl_state));

              did_recurse = TRUE;

              gtk_file_selection_populate (fs, dir_name, TRUE, TRUE);

              g_free (dir_name);
            }
          else
            {
	      if (fs->selection_entry)
		      gtk_entry_set_text (GTK_ENTRY (fs->selection_entry),
					  cmpl_updated_text (cmpl_state));
            }
        }
      else
        {
	  if (fs->selection_entry)
	    gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), rem_path);
        }
    }
  else if (reset_entry)
    {
      if (fs->selection_entry)
	gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), "");
    }

  if (!did_recurse)
    {
      if (fs->selection_entry && try_complete)
	gtk_editable_set_position (GTK_EDITABLE (fs->selection_entry), -1);

      if (fs->selection_entry)
	{
	  char *escaped = escape_underscores (cmpl_reference_position (cmpl_state));
	  sel_text = g_strconcat (_("_Selection: "), escaped, NULL);
	  g_free (escaped);

	  gtk_label_set_text_with_mnemonic (GTK_LABEL (fs->selection_text), sel_text);
	  g_free (sel_text);
	}

      if (fs->history_pulldown) 
	{
	  gtk_file_selection_update_history_menu (fs, cmpl_reference_position (cmpl_state));
	}
      
    }
}

static void
gtk_file_selection_abort (GtkFileSelection *fs)
{
  gchar err_buf[256];

  g_snprintf (err_buf, sizeof (err_buf), _("Folder unreadable: %s"), cmpl_strerror (cmpl_errno));

  /*  BEEP gdk_beep();  */

  if (fs->selection_entry)
    gtk_label_set_text (GTK_LABEL (fs->selection_text), err_buf);
}

/**
 * gtk_file_selection_set_select_multiple:
 * @filesel: a #GtkFileSelection
 * @select_multiple: whether or not the user is allowed to select multiple
 * files in the file list.
 *
 * Sets whether the user is allowed to select multiple files in the file list.
 * Use gtk_file_selection_get_selections () to get the list of selected files.
 **/
void
gtk_file_selection_set_select_multiple (GtkFileSelection *filesel,
					gboolean          select_multiple)
{
  GtkTreeSelection *sel;
  GtkSelectionMode mode;

  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (filesel->file_list));

  mode = select_multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE;

  if (mode != gtk_tree_selection_get_mode (sel))
    {
      gtk_tree_selection_set_mode (sel, mode);

      g_object_notify (G_OBJECT (filesel), "select-multiple");
    }
}

/**
 * gtk_file_selection_get_select_multiple:
 * @filesel: a #GtkFileSelection
 *
 * Determines whether or not the user is allowed to select multiple files in
 * the file list. See gtk_file_selection_set_select_multiple().
 *
 * Return value: %TRUE if the user is allowed to select multiple files in the
 * file list
 **/
gboolean
gtk_file_selection_get_select_multiple (GtkFileSelection *filesel)
{
  GtkTreeSelection *sel;

  g_return_val_if_fail (GTK_IS_FILE_SELECTION (filesel), FALSE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (filesel->file_list));
  return (gtk_tree_selection_get_mode (sel) == GTK_SELECTION_MULTIPLE);
}

static void
multiple_changed_foreach (GtkTreeModel *model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter,
			  gpointer      data)
{
  GPtrArray *names = data;
  gchar *filename;

  gtk_tree_model_get (model, iter, FILE_COLUMN, &filename, -1);

  g_ptr_array_add (names, filename);
}

static void
free_selected_names (GPtrArray *names)
{
  gint i;

  for (i = 0; i < names->len; i++)
    g_free (g_ptr_array_index (names, i));

  g_ptr_array_free (names, TRUE);
}

static void
gtk_file_selection_file_changed (GtkTreeSelection *selection,
				 gpointer          user_data)
{
  GtkFileSelection *fs = GTK_FILE_SELECTION (user_data);
  GPtrArray *new_names;
  gchar *filename;
  const gchar *entry;
  gint index = -1;

  new_names = g_ptr_array_sized_new (8);

  gtk_tree_selection_selected_foreach (selection,
				       multiple_changed_foreach,
				       new_names);

  /* nothing selected */
  if (new_names->len == 0)
    {
      g_ptr_array_free (new_names, TRUE);

      if (fs->selected_names != NULL)
	{
	  free_selected_names (fs->selected_names);
	  fs->selected_names = NULL;
	}

      goto maybe_clear_entry;
    }

  if (new_names->len != 1)
    {
      GPtrArray *old_names = fs->selected_names;

      if (old_names != NULL)
	{
	  /* A common case is selecting a range of files from top to bottom,
	   * so quickly check for that to avoid looping over the entire list
	   */
	  if (compare_utf8_filenames (g_ptr_array_index (old_names, old_names->len - 1),
				      g_ptr_array_index (new_names, new_names->len - 1)) != 0)
	    index = new_names->len - 1;
	  else
	    {
	      gint i = 0, j = 0, cmp;

	      /* do a quick diff, stopping at the first file not in the
	       * old list
	       */
	      while (i < old_names->len && j < new_names->len)
		{
		  cmp = compare_utf8_filenames (g_ptr_array_index (old_names, i),
						g_ptr_array_index (new_names, j));
		  if (cmp < 0)
		    {
		      i++;
		    }
		  else if (cmp == 0)
		    {
		      i++;
		      j++;
		    }
		  else if (cmp > 0)
		    {
		      index = j;
		      break;
		    }
		}

	      /* we ran off the end of the old list */
	      if (index == -1 && i < new_names->len)
		index = j;
	    }
	}
      else
	{
	  /* A phantom anchor still exists at the point where the last item
	   * was selected, which is used for subsequent range selections.
	   * So search up from there.
	   */
	  if (fs->last_selected &&
	      compare_utf8_filenames (fs->last_selected,
				      g_ptr_array_index (new_names, 0)) == 0)
	    index = new_names->len - 1;
	  else
	    index = 0;
	}
    }
  else
    index = 0;

  if (fs->selected_names != NULL)
    free_selected_names (fs->selected_names);

  fs->selected_names = new_names;

  if (index != -1)
    {
      g_free (fs->last_selected);

      fs->last_selected = g_strdup (g_ptr_array_index (new_names, index));
      filename = get_real_filename (fs->last_selected, FALSE);

      gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);

      if (filename != fs->last_selected)
	g_free (filename);
      
      return;
    }
  
maybe_clear_entry:

  entry = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));
  if ((entry != NULL) && (fs->last_selected != NULL) &&
      (compare_utf8_filenames (entry, fs->last_selected) == 0))
    gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), "");
}

/**
 * gtk_file_selection_get_selections:
 * @filesel: a #GtkFileSelection
 *
 * Retrieves the list of file selections the user has made in the dialog box.
 * This function is intended for use when the user can select multiple files
 * in the file list. 
 *
 * The filenames are in the GLib file name encoding. To convert to
 * UTF-8, call g_filename_to_utf8() on each string.
 *
 * Return value: a newly-allocated %NULL-terminated array of strings. Use
 * g_strfreev() to free it.
 **/
gchar **
gtk_file_selection_get_selections (GtkFileSelection *filesel)
{
  GPtrArray *names;
  gchar **selections;
  gchar *filename, *dirname;
  gchar *current, *buf;
  gint i, count;
  gboolean unselected_entry;

  g_return_val_if_fail (GTK_IS_FILE_SELECTION (filesel), NULL);

  filename = g_strdup (gtk_file_selection_get_filename (filesel));

  if (strlen (filename) == 0)
    {
      g_free (filename);
      return NULL;
    }

  names = filesel->selected_names;

  if (names != NULL)
    selections = g_new (gchar *, names->len + 2);
  else
    selections = g_new (gchar *, 2);

  count = 0;
  unselected_entry = TRUE;

  if (names != NULL)
    {
      dirname = g_path_get_dirname (filename);

      if ((names->len >= 1) && 
	  (strcmp (gtk_entry_get_text (GTK_ENTRY (filesel->selection_entry)), "") == 0))
	{ /* multiple files are selected and last selection was removed via ctrl click */
	  g_free (dirname);
	  dirname = g_strdup (filename); /* as gtk_file_selection_get_filename returns dir 
					    if no file is selected */
	  unselected_entry = FALSE;
	}

      for (i = 0; i < names->len; i++)
	{
	  buf = g_filename_from_utf8 (g_ptr_array_index (names, i), -1,
				      NULL, NULL, NULL);
	  current = g_build_filename (dirname, buf, NULL);
	  g_free (buf);

	  selections[count++] = current;

	  if (unselected_entry && compare_sys_filenames (current, filename) == 0)
	    unselected_entry = FALSE;
	}

      g_free (dirname);
    }

  if (unselected_entry)
    selections[count++] = filename;
  else
    g_free (filename);

  selections[count] = NULL;

  return selections;
}

/**********************************************************************/
/*			  External Interface                          */
/**********************************************************************/

/* The four completion state selectors
 */
static gchar*
cmpl_updated_text (CompletionState *cmpl_state)
{
  return cmpl_state->updated_text;
}

static gboolean
cmpl_updated_dir (CompletionState *cmpl_state)
{
  return cmpl_state->re_complete;
}

static gchar*
cmpl_reference_position (CompletionState *cmpl_state)
{
  return cmpl_state->reference_dir->fullname;
}

#if 0
/* This doesn't work currently and would require changes
 * to fnmatch.c to get working.
 */
static gint
cmpl_last_valid_char (CompletionState *cmpl_state)
{
  return cmpl_state->last_valid_char;
}
#endif

static gchar*
cmpl_completion_fullname (const gchar     *text,
			  CompletionState *cmpl_state)
{
  if (!cmpl_state_okay (cmpl_state))
    {
      return g_strdup ("");
    }
  else if (g_path_is_absolute (text))
    {
      return g_strdup (text);
    }
#ifdef HAVE_PWD_H
  else if (text[0] == '~')
    {
      CompletionDir* dir;
      char* slash;

      dir = open_user_dir (text, cmpl_state);

      if (dir)
	{
	  slash = strchr (text, G_DIR_SEPARATOR);
	  
	  /* slash may be NULL, that works too */
	  return g_strconcat (dir->fullname, slash, NULL);
	}
    }
#endif
  
  return g_build_filename (cmpl_state->reference_dir->fullname,
			   text,
			   NULL);
}

/* The three completion selectors
 */
static gchar*
cmpl_this_completion (PossibleCompletion* pc)
{
  return pc->text;
}

static gboolean
cmpl_is_directory (PossibleCompletion* pc)
{
  return pc->is_directory;
}

static gint
cmpl_is_a_completion (PossibleCompletion* pc)
{
  return pc->is_a_completion;
}

/**********************************************************************/
/*	                 Construction, deletion                       */
/**********************************************************************/

/* Get the nearest parent of the current directory for which
 * we can convert the filename into UTF-8. With paranoia.
 * Returns "." when all goes wrong.
 */
static gchar *
get_current_dir_utf8 (void)
{
  gchar *dir = g_get_current_dir ();
  gchar *dir_utf8 = NULL;

  while (TRUE)
    {
      gchar *last_slash;

      dir_utf8 = g_filename_to_utf8 (dir, -1, NULL, NULL, NULL);
      if (dir_utf8)
	break;

      last_slash = strrchr (dir, G_DIR_SEPARATOR);
      if (!last_slash)		/* g_get_current_dir() wasn't absolute! */
	break;

      if (last_slash + 1 == g_path_skip_root (dir)) /* Parent directory is a root directory */
	{
	  if (last_slash[1] == '\0') /* Root misencoded! */
	    break;
	  else
	    last_slash[1] = '\0';
	}
      else
	last_slash[0] = '\0';
      
      g_assert (last_slash);
    }

  g_free (dir);
  
  return dir_utf8 ? dir_utf8 : g_strdup (".");
}

static CompletionState*
cmpl_init_state (void)
{
  gchar *utf8_cwd;
  CompletionState *new_state;
  gint tries = 0;

  new_state = g_new (CompletionState, 1);

  utf8_cwd = get_current_dir_utf8 ();

tryagain:
  tries++;
  new_state->reference_dir = NULL;
  new_state->completion_dir = NULL;
  new_state->active_completion_dir = NULL;
  new_state->directory_storage = NULL;
  new_state->directory_sent_storage = NULL;
  new_state->last_valid_char = 0;
  new_state->updated_text = g_new (gchar, MAXPATHLEN);
  new_state->updated_text_alloc = MAXPATHLEN;
  new_state->the_completion.text = g_new (gchar, MAXPATHLEN);
  new_state->the_completion.text_alloc = MAXPATHLEN;
  new_state->user_dir_name_buffer = NULL;
  new_state->user_directories = NULL;

  new_state->reference_dir = open_dir (utf8_cwd, new_state);

  if (!new_state->reference_dir)
    {
      /* Directories changing from underneath us, grumble */
      strcpy (utf8_cwd, G_DIR_SEPARATOR_S);
      if (tries < 2)
	goto tryagain;
    }

  g_free (utf8_cwd);
  return new_state;
}

static void
cmpl_free_dir_list (GList* dp0)
{
  GList *dp = dp0;

  while (dp)
    {
      free_dir (dp->data);
      dp = dp->next;
    }

  g_list_free (dp0);
}

static void
cmpl_free_dir_sent_list (GList* dp0)
{
  GList *dp = dp0;

  while (dp)
    {
      free_dir_sent (dp->data);
      dp = dp->next;
    }

  g_list_free (dp0);
}

static void
cmpl_free_state (CompletionState* cmpl_state)
{
  g_return_if_fail (cmpl_state != NULL);

  cmpl_free_dir_list (cmpl_state->directory_storage);
  cmpl_free_dir_sent_list (cmpl_state->directory_sent_storage);

  g_free (cmpl_state->user_dir_name_buffer);
  g_free (cmpl_state->user_directories);
  g_free (cmpl_state->the_completion.text);
  g_free (cmpl_state->updated_text);

  g_free (cmpl_state);
}

static void
free_dir (CompletionDir* dir)
{
  g_free (dir->cmpl_text);
  g_free (dir->fullname);
  g_free (dir);
}

static void
free_dir_sent (CompletionDirSent* sent)
{
  gint i;
  for (i = 0; i < sent->entry_count; i++)
    {
      g_free (sent->entries[i].entry_name);
      g_free (sent->entries[i].sort_key);
    }
  g_free (sent->entries);
  g_free (sent);
}

static void
prune_memory_usage (CompletionState *cmpl_state)
{
  GList* cdsl = cmpl_state->directory_sent_storage;
  GList* cdl = cmpl_state->directory_storage;
  GList* cdl0 = cdl;
  gint len = 0;

  for (; cdsl && len < CMPL_DIRECTORY_CACHE_SIZE; len += 1)
    cdsl = cdsl->next;

  if (cdsl)
    {
      cmpl_free_dir_sent_list (cdsl->next);
      cdsl->next = NULL;
    }

  cmpl_state->directory_storage = NULL;
  while (cdl)
    {
      if (cdl->data == cmpl_state->reference_dir)
	cmpl_state->directory_storage = g_list_prepend (NULL, cdl->data);
      else
	free_dir (cdl->data);
      cdl = cdl->next;
    }

  g_list_free (cdl0);
}

/**********************************************************************/
/*                        The main entrances.                         */
/**********************************************************************/

static PossibleCompletion*
cmpl_completion_matches (gchar           *text_to_complete,
			 gchar          **remaining_text,
			 CompletionState *cmpl_state)
{
#ifdef HAVE_PWD_H
  gchar* first_slash;
#endif
  PossibleCompletion *poss;

  prune_memory_usage (cmpl_state);

  g_assert (text_to_complete != NULL);

  cmpl_state->user_completion_index = -1;
  cmpl_state->last_completion_text = text_to_complete;
  cmpl_state->the_completion.text[0] = 0;
  cmpl_state->last_valid_char = 0;
  cmpl_state->updated_text_len = -1;
  cmpl_state->updated_text[0] = 0;
  cmpl_state->re_complete = FALSE;

#ifdef HAVE_PWD_H
  first_slash = strchr (text_to_complete, G_DIR_SEPARATOR);

  if (text_to_complete[0] == '~' && !first_slash)
    {
      /* Text starts with ~ and there is no slash, show all the
       * home directory completions.
       */
      poss = attempt_homedir_completion (text_to_complete, cmpl_state);

      update_cmpl (poss, cmpl_state);

      return poss;
    }
#endif
  cmpl_state->reference_dir =
    open_ref_dir (text_to_complete, remaining_text, cmpl_state);

  if (!cmpl_state->reference_dir)
    return NULL;

  cmpl_state->completion_dir =
    find_completion_dir (*remaining_text, remaining_text, cmpl_state);

  cmpl_state->last_valid_char = *remaining_text - text_to_complete;

  if (!cmpl_state->completion_dir)
    return NULL;

  cmpl_state->completion_dir->cmpl_index = -1;
  cmpl_state->completion_dir->cmpl_parent = NULL;
  cmpl_state->completion_dir->cmpl_text = g_strdup (*remaining_text);

  cmpl_state->active_completion_dir = cmpl_state->completion_dir;

  cmpl_state->reference_dir = cmpl_state->completion_dir;

  poss = attempt_file_completion (cmpl_state);

  update_cmpl (poss, cmpl_state);

  return poss;
}

static PossibleCompletion*
cmpl_next_completion (CompletionState* cmpl_state)
{
  PossibleCompletion* poss = NULL;

  cmpl_state->the_completion.text[0] = 0;

#ifdef HAVE_PWD_H
  if (cmpl_state->user_completion_index >= 0)
    poss = attempt_homedir_completion (cmpl_state->last_completion_text, cmpl_state);
  else
    poss = attempt_file_completion (cmpl_state);
#else
  poss = attempt_file_completion (cmpl_state);
#endif

  update_cmpl (poss, cmpl_state);

  return poss;
}

/**********************************************************************/
/*			 Directory Operations                         */
/**********************************************************************/

/* Open the directory where completion will begin from, if possible. */
static CompletionDir*
open_ref_dir (gchar           *text_to_complete,
	      gchar          **remaining_text,
	      CompletionState *cmpl_state)
{
  gchar* first_slash;
  CompletionDir *new_dir;

  first_slash = strchr (text_to_complete, G_DIR_SEPARATOR);

#ifdef G_WITH_CYGWIN
  if (text_to_complete[0] == '/' && text_to_complete[1] == '/')
    {
      char root_dir[5];
      g_snprintf (root_dir, sizeof (root_dir), "//%c", text_to_complete[2]);

      new_dir = open_dir (root_dir, cmpl_state);

      if (new_dir) {
	*remaining_text = text_to_complete + 4;
      }
    }
#else
  if (FALSE)
    ;
#endif
#ifdef HAVE_PWD_H
  else if (text_to_complete[0] == '~')
    {
      new_dir = open_user_dir (text_to_complete, cmpl_state);

      if (new_dir)
	{
	  if (first_slash)
	    *remaining_text = first_slash + 1;
	  else
	    *remaining_text = text_to_complete + strlen (text_to_complete);
	}
      else
	{
	  return NULL;
	}
    }
#endif
  else if (g_path_is_absolute (text_to_complete) || !cmpl_state->reference_dir)
    {
      gchar *tmp = g_strdup (text_to_complete);
      gchar *p;

      p = tmp;
      while (*p && *p != '*' && *p != '?')
	p++;

      *p = '\0';
      p = strrchr (tmp, G_DIR_SEPARATOR);
      if (p)
	{
	  if (p + 1 == g_path_skip_root (tmp))
	    p++;
      
	  *p = '\0';
	  new_dir = open_dir (tmp, cmpl_state);

	  if (new_dir)
	    *remaining_text = text_to_complete + 
	      ((p == g_path_skip_root (tmp)) ? (p - tmp) : (p + 1 - tmp));
	}
      else
	{
	  /* If no possible candidates, use the cwd */
	  gchar *utf8_curdir = get_current_dir_utf8 ();

	  new_dir = open_dir (utf8_curdir, cmpl_state);

	  if (new_dir)
	    *remaining_text = text_to_complete;

	  g_free (utf8_curdir);
	}

      g_free (tmp);
    }
  else
    {
      *remaining_text = text_to_complete;

      new_dir = open_dir (cmpl_state->reference_dir->fullname, cmpl_state);
    }

  if (new_dir)
    {
      new_dir->cmpl_index = -1;
      new_dir->cmpl_parent = NULL;
    }

  return new_dir;
}

#ifdef HAVE_PWD_H

/* open a directory by user name */
static CompletionDir*
open_user_dir (const gchar     *text_to_complete,
	       CompletionState *cmpl_state)
{
  CompletionDir *result;
  gchar *first_slash;
  gint cmp_len;

  g_assert (text_to_complete && text_to_complete[0] == '~');

  first_slash = strchr (text_to_complete, G_DIR_SEPARATOR);

  if (first_slash)
    cmp_len = first_slash - text_to_complete - 1;
  else
    cmp_len = strlen (text_to_complete + 1);

  if (!cmp_len)
    {
      /* ~/ */
      const gchar *homedir = g_get_home_dir ();
      gchar *utf8_homedir = g_filename_to_utf8 (homedir, -1, NULL, NULL, NULL);

      if (utf8_homedir)
	result = open_dir (utf8_homedir, cmpl_state);
      else
	result = NULL;
      
      g_free (utf8_homedir);
    }
  else
    {
      /* ~user/ */
      gchar* copy = g_new (char, cmp_len + 1);
      gchar *utf8_dir;
      struct passwd *pwd;

      strncpy (copy, text_to_complete + 1, cmp_len);
      copy[cmp_len] = 0;
      pwd = getpwnam (copy);
      g_free (copy);
      if (!pwd)
	{
	  cmpl_errno = errno;
	  return NULL;
	}
      utf8_dir = g_filename_to_utf8 (pwd->pw_dir, -1, NULL, NULL, NULL);
      result = open_dir (utf8_dir, cmpl_state);
      g_free (utf8_dir);
    }
  return result;
}

#endif

/* open a directory relative to the current relative directory */
static CompletionDir*
open_relative_dir (gchar           *dir_name,
		   CompletionDir   *dir,
		   CompletionState *cmpl_state)
{
  CompletionDir *result;
  GString *path;

  path = g_string_sized_new (dir->fullname_len + strlen (dir_name) + 10);
  g_string_assign (path, dir->fullname);

  if (dir->fullname_len > 1
      && path->str[dir->fullname_len - 1] != G_DIR_SEPARATOR)
    g_string_append_c (path, G_DIR_SEPARATOR);
  g_string_append (path, dir_name);

  result = open_dir (path->str, cmpl_state);

  g_string_free (path, TRUE);

  return result;
}

/* after the cache lookup fails, really open a new directory */
static CompletionDirSent*
open_new_dir (gchar       *dir_name,
	      struct stat *sbuf,
	      gboolean     stat_subdirs)
{
  CompletionDirSent *sent;
  GDir *directory;
  const char *dirent;
  GError *error = NULL;
  gint entry_count = 0;
  gint n_entries = 0;
  gint i;
  struct stat ent_sbuf;
  GString *path;
  gchar *sys_dir_name;

  sent = g_new (CompletionDirSent, 1);
#ifndef G_PLATFORM_WIN32
  sent->mtime = sbuf->st_mtime;
  sent->inode = sbuf->st_ino;
  sent->device = sbuf->st_dev;
#endif
  path = g_string_sized_new (2*MAXPATHLEN + 10);

  sys_dir_name = g_filename_from_utf8 (dir_name, -1, NULL, NULL, NULL);
  if (!sys_dir_name)
    {
      cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
      g_free (sent);
      return NULL;
    }

  directory = g_dir_open (sys_dir_name, 0, &error);
  if (!directory)
    {
      cmpl_errno = error->code; /* ??? */
      g_free (sys_dir_name);
      g_free (sent);
      return NULL;
    }

  while ((dirent = g_dir_read_name (directory)) != NULL)
    entry_count++;
  entry_count += 2;		/* For ".",".." */

  sent->entries = g_new (CompletionDirEntry, entry_count);
  sent->entry_count = entry_count;

  g_dir_rewind (directory);

  for (i = 0; i < entry_count; i += 1)
    {
      GError *error = NULL;

      if (i == 0)
	dirent = ".";
      else if (i == 1)
	dirent = "..";
      else
	{
	  dirent = g_dir_read_name (directory);
	  if (!dirent)		/* Directory changed */
	    break;
	}

      sent->entries[n_entries].entry_name = g_filename_to_utf8 (dirent, -1, NULL, NULL, &error);
      if (sent->entries[n_entries].entry_name == NULL
	  || !g_utf8_validate (sent->entries[n_entries].entry_name, -1, NULL))
	{
	  gchar *escaped_str = g_strescape (dirent, NULL);
	  g_message (_("The filename \"%s\" couldn't be converted to UTF-8. "
		       "(try setting the environment variable G_FILENAME_ENCODING): %s"),
		     escaped_str,
		     error->message ? error->message : _("Invalid UTF-8"));
	  g_free (escaped_str);
	  g_clear_error (&error);
	  continue;
	}
      g_clear_error (&error);
      
      sent->entries[n_entries].sort_key = g_utf8_collate_key (sent->entries[n_entries].entry_name, -1);
      
      g_string_assign (path, sys_dir_name);
      if (path->str[path->len-1] != G_DIR_SEPARATOR)
	{
	  g_string_append_c (path, G_DIR_SEPARATOR);
	}
      g_string_append (path, dirent);

      if (stat_subdirs)
	{
	  /* Here we know path->str is a "system charset" string */
	  if (g_stat (path->str, &ent_sbuf) >= 0 && S_ISDIR (ent_sbuf.st_mode))
	    sent->entries[n_entries].is_dir = TRUE;
	  else
	    /* stat may fail, and we don't mind, since it could be a
	     * dangling symlink. */
	    sent->entries[n_entries].is_dir = FALSE;
	}
      else
	sent->entries[n_entries].is_dir = 1;

      n_entries++;
    }
  sent->entry_count = n_entries;
  
  g_free (sys_dir_name);
  g_string_free (path, TRUE);
  qsort (sent->entries, sent->entry_count, sizeof (CompletionDirEntry), compare_cmpl_dir);

  g_dir_close (directory);

  return sent;
}

#ifndef G_PLATFORM_WIN32

static gboolean
check_dir (gchar       *dir_name,
	   struct stat *result,
	   gboolean    *stat_subdirs)
{
  /* A list of directories that we know only contain other directories.
   * Trying to stat every file in these directories would be very
   * expensive.
   */

  static struct {
    const gchar name[5];
    gboolean present;
    struct stat statbuf;
  } no_stat_dirs[] = {
    { "/afs", FALSE, { 0 } },
    { "/net", FALSE, { 0 } }
  };

  static const gint n_no_stat_dirs = G_N_ELEMENTS (no_stat_dirs);
  static gboolean initialized = FALSE;
  gchar *sys_dir_name;
  gint i;

  if (!initialized)
    {
      initialized = TRUE;
      for (i = 0; i < n_no_stat_dirs; i++)
	{
	  if (g_stat (no_stat_dirs[i].name, &no_stat_dirs[i].statbuf) == 0)
	    no_stat_dirs[i].present = TRUE;
	}
    }

  sys_dir_name = g_filename_from_utf8 (dir_name, -1, NULL, NULL, NULL);
  if (!sys_dir_name)
    {
      cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
      return FALSE;
    }
  
  if (g_stat (sys_dir_name, result) < 0)
    {
      g_free (sys_dir_name);
      cmpl_errno = errno;
      return FALSE;
    }
  g_free (sys_dir_name);

  *stat_subdirs = TRUE;
  for (i = 0; i < n_no_stat_dirs; i++)
    {
      if (no_stat_dirs[i].present &&
	  (no_stat_dirs[i].statbuf.st_dev == result->st_dev) &&
	  (no_stat_dirs[i].statbuf.st_ino == result->st_ino))
	{
	  *stat_subdirs = FALSE;
	  break;
	}
    }

  return TRUE;
}

#endif

/* open a directory by absolute pathname */
static CompletionDir*
open_dir (gchar           *dir_name,
	  CompletionState *cmpl_state)
{
#ifndef G_PLATFORM_WIN32
  struct stat sbuf;
  gboolean stat_subdirs;
  GList* cdsl;
#endif
  CompletionDirSent *sent;

#ifndef G_PLATFORM_WIN32
  if (!check_dir (dir_name, &sbuf, &stat_subdirs))
    return NULL;

  cdsl = cmpl_state->directory_sent_storage;

  while (cdsl)
    {
      sent = cdsl->data;

      if (sent->inode == sbuf.st_ino &&
	  sent->mtime == sbuf.st_mtime &&
	  sent->device == sbuf.st_dev)
	return attach_dir (sent, dir_name, cmpl_state);

      cdsl = cdsl->next;
    }

  sent = open_new_dir (dir_name, &sbuf, stat_subdirs);
#else
  sent = open_new_dir (dir_name, NULL, TRUE);
#endif

  if (sent)
    {
      cmpl_state->directory_sent_storage =
	g_list_prepend (cmpl_state->directory_sent_storage, sent);

      return attach_dir (sent, dir_name, cmpl_state);
    }

  return NULL;
}

static CompletionDir*
attach_dir (CompletionDirSent *sent,
	    gchar             *dir_name,
	    CompletionState   *cmpl_state)
{
  CompletionDir* new_dir;

  new_dir = g_new (CompletionDir, 1);

  cmpl_state->directory_storage =
    g_list_prepend (cmpl_state->directory_storage, new_dir);

  new_dir->sent = sent;
  new_dir->fullname = g_strdup (dir_name);
  new_dir->fullname_len = strlen (dir_name);
  new_dir->cmpl_text = NULL;

  return new_dir;
}

static gint
correct_dir_fullname (CompletionDir* cmpl_dir)
{
  gint length = strlen (cmpl_dir->fullname);
  gchar *first_slash = strchr (cmpl_dir->fullname, G_DIR_SEPARATOR);
  gchar *sys_filename;
  struct stat sbuf;

  /* Does it end with /. (\.) ? */
  if (length >= 2 &&
      strcmp (cmpl_dir->fullname + length - 2, G_DIR_SEPARATOR_S ".") == 0)
    {
      /* Is it just the root directory (on a drive) ? */
      if (cmpl_dir->fullname + length - 2 == first_slash)
	{
	  cmpl_dir->fullname[length - 1] = 0;
	  cmpl_dir->fullname_len = length - 1;
	  return TRUE;
	}
      else
	{
	  cmpl_dir->fullname[length - 2] = 0;
	}
    }

  /* Ends with /./ (\.\)? */
  else if (length >= 3 &&
	   strcmp (cmpl_dir->fullname + length - 3,
		   G_DIR_SEPARATOR_S "." G_DIR_SEPARATOR_S) == 0)
    cmpl_dir->fullname[length - 2] = 0;

  /* Ends with /.. (\..) ? */
  else if (length >= 3 &&
	   strcmp (cmpl_dir->fullname + length - 3,
		   G_DIR_SEPARATOR_S "..") == 0)
    {
      /* Is it just /.. (X:\..)? */
      if (cmpl_dir->fullname + length - 3 == first_slash)
	{
	  cmpl_dir->fullname[length - 2] = 0;
	  cmpl_dir->fullname_len = length - 2;
	  return TRUE;
	}

      sys_filename = g_filename_from_utf8 (cmpl_dir->fullname, -1, NULL, NULL, NULL);
      if (!sys_filename)
	{
	  cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
	  return FALSE;
	}
      
      if (g_stat (sys_filename, &sbuf) < 0)
	{
	  g_free (sys_filename);
	  cmpl_errno = errno;
	  return FALSE;
	}
      g_free (sys_filename);

      cmpl_dir->fullname[length - 3] = 0;

      if (!correct_parent (cmpl_dir, &sbuf))
	return FALSE;
    }

  /* Ends with /../ (\..\)? */
  else if (length >= 4 &&
	   strcmp (cmpl_dir->fullname + length - 4,
		   G_DIR_SEPARATOR_S ".." G_DIR_SEPARATOR_S) == 0)
    {
      /* Is it just /../ (X:\..\)? */
      if (cmpl_dir->fullname + length - 4 == first_slash)
	{
	  cmpl_dir->fullname[length - 3] = 0;
	  cmpl_dir->fullname_len = length - 3;
	  return TRUE;
	}

      sys_filename = g_filename_from_utf8 (cmpl_dir->fullname, -1, NULL, NULL, NULL);
      if (!sys_filename)
	{
	  cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
	  return FALSE;
	}
      
      if (g_stat (sys_filename, &sbuf) < 0)
	{
	  g_free (sys_filename);
	  cmpl_errno = errno;
	  return FALSE;
	}
      g_free (sys_filename);

      cmpl_dir->fullname[length - 4] = 0;

      if (!correct_parent (cmpl_dir, &sbuf))
	return FALSE;
    }

  cmpl_dir->fullname_len = strlen (cmpl_dir->fullname);

  return TRUE;
}

static gint
correct_parent (CompletionDir *cmpl_dir,
		struct stat   *sbuf)
{
  struct stat parbuf;
  gchar *last_slash;
  gchar *first_slash;
#ifndef G_PLATFORM_WIN32
  gchar *new_name;
#endif
  gchar *sys_filename;
  gchar c = 0;

  last_slash = strrchr (cmpl_dir->fullname, G_DIR_SEPARATOR);
  g_assert (last_slash);
  first_slash = strchr (cmpl_dir->fullname, G_DIR_SEPARATOR);

  /* Clever (?) way to check for top-level directory that works also on
   * Win32, where there is a drive letter and colon prefixed...
   */
  if (last_slash != first_slash)
    {
      last_slash[0] = 0;
    }
  else
    {
      c = last_slash[1];
      last_slash[1] = 0;
    }

  sys_filename = g_filename_from_utf8 (cmpl_dir->fullname, -1, NULL, NULL, NULL);
  if (!sys_filename)
    {
      cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
      if (!c)
	last_slash[0] = G_DIR_SEPARATOR;
      return FALSE;
    }
  
  if (g_stat (sys_filename, &parbuf) < 0)
    {
      g_free (sys_filename);
      cmpl_errno = errno;
      if (!c)
	last_slash[0] = G_DIR_SEPARATOR;
      return FALSE;
    }
  g_free (sys_filename);

#ifndef G_PLATFORM_WIN32	/* No inode numbers on Win32 */
  if (parbuf.st_ino == sbuf->st_ino && parbuf.st_dev == sbuf->st_dev)
    /* it wasn't a link */
    return TRUE;

  if (c)
    last_slash[1] = c;
  else
    last_slash[0] = G_DIR_SEPARATOR;

  /* it was a link, have to figure it out the hard way */

  new_name = find_parent_dir_fullname (cmpl_dir->fullname);

  if (!new_name)
    return FALSE;

  g_free (cmpl_dir->fullname);

  cmpl_dir->fullname = new_name;
#endif

  return TRUE;
}

#ifndef G_PLATFORM_WIN32

static gchar*
find_parent_dir_fullname (gchar* dirname)
{
  gchar *sys_orig_dir;
  gchar *result;
  gchar *sys_cwd;
  gchar *sys_dirname;

  sys_orig_dir = g_get_current_dir ();
  sys_dirname = g_filename_from_utf8 (dirname, -1, NULL, NULL, NULL);
  if (!sys_dirname)
    {
      g_free (sys_orig_dir);
      cmpl_errno = CMPL_ERRNO_DID_NOT_CONVERT;
      return NULL;
    }
  
  if (chdir (sys_dirname) != 0 || chdir ("..") != 0)
    {
      int ignored;

      cmpl_errno = errno;
      ignored = g_chdir (sys_orig_dir);
      g_free (sys_dirname);
      g_free (sys_orig_dir);
      return NULL;
    }
  g_free (sys_dirname);

  sys_cwd = g_get_current_dir ();
  result = g_filename_to_utf8 (sys_cwd, -1, NULL, NULL, NULL);
  g_free (sys_cwd);

  if (chdir (sys_orig_dir) != 0)
    {
      cmpl_errno = errno;
      g_free (sys_orig_dir);
      return NULL;
    }

  g_free (sys_orig_dir);
  return result;
}

#endif

/**********************************************************************/
/*                        Completion Operations                       */
/**********************************************************************/

#ifdef HAVE_PWD_H

static PossibleCompletion*
attempt_homedir_completion (gchar           *text_to_complete,
			    CompletionState *cmpl_state)
{
  gint index;

  if (!cmpl_state->user_dir_name_buffer &&
      !get_pwdb (cmpl_state))
    return NULL;

  cmpl_state->user_completion_index += 1;

  while (cmpl_state->user_completion_index < cmpl_state->user_directories_len)
    {
      index = first_diff_index (text_to_complete + 1,
				cmpl_state->user_directories
				[cmpl_state->user_completion_index].login);

      switch (index)
	{
	case PATTERN_MATCH:
	  break;
	default:
	  if (cmpl_state->last_valid_char < (index + 1))
	    cmpl_state->last_valid_char = index + 1;
	  cmpl_state->user_completion_index += 1;
	  continue;
	}

      cmpl_state->the_completion.is_a_completion = 1;
      cmpl_state->the_completion.is_directory = TRUE;

      append_completion_text ("~", cmpl_state);

      append_completion_text (cmpl_state->
			      user_directories[cmpl_state->user_completion_index].login,
			      cmpl_state);

      return append_completion_text (G_DIR_SEPARATOR_S, cmpl_state);
    }

  if (text_to_complete[1]
      || cmpl_state->user_completion_index > cmpl_state->user_directories_len)
    {
      cmpl_state->user_completion_index = -1;
      return NULL;
    }
  else
    {
      cmpl_state->user_completion_index += 1;
      cmpl_state->the_completion.is_a_completion = 1;
      cmpl_state->the_completion.is_directory = TRUE;

      return append_completion_text ("~" G_DIR_SEPARATOR_S, cmpl_state);
    }
}

#endif

#ifdef G_PLATFORM_WIN32
/* FIXME: determine whether we should casefold all Unicode letters
 * here, too (and in in first_diff_index() walk through the strings with
 * g_utf8_next_char()), or if this folding isn't actually needed at
 * all.
 */
#define FOLD(c) (tolower(c))
#else
#define FOLD(c) (c)
#endif

/* returns the index (>= 0) of the first differing character,
 * PATTERN_MATCH if the completion matches */
static gint
first_diff_index (gchar *pat,
		  gchar *text)
{
  gint diff = 0;

  while (*pat && *text && FOLD (*text) == FOLD (*pat))
    {
      pat += 1;
      text += 1;
      diff += 1;
    }

  if (*pat)
    return diff;

  return PATTERN_MATCH;
}

static PossibleCompletion*
append_completion_text (gchar           *text,
			CompletionState *cmpl_state)
{
  gint len, i = 1;

  if (!cmpl_state->the_completion.text)
    return NULL;

  len = strlen (text) + strlen (cmpl_state->the_completion.text) + 1;

  if (cmpl_state->the_completion.text_alloc > len)
    {
      strcat (cmpl_state->the_completion.text, text);
      return &cmpl_state->the_completion;
    }

  while (i < len)
    i <<= 1;

  cmpl_state->the_completion.text_alloc = i;

  cmpl_state->the_completion.text = (gchar*) g_realloc (cmpl_state->the_completion.text, i);

  if (!cmpl_state->the_completion.text)
    return NULL;
  else
    {
      strcat (cmpl_state->the_completion.text, text);
      return &cmpl_state->the_completion;
    }
}

static CompletionDir*
find_completion_dir (gchar          *text_to_complete,
		    gchar          **remaining_text,
		    CompletionState *cmpl_state)
{
  gchar* first_slash = strchr (text_to_complete, G_DIR_SEPARATOR);
  CompletionDir* dir = cmpl_state->reference_dir;
  CompletionDir* next;
  *remaining_text = text_to_complete;

  while (first_slash)
    {
      gint len = first_slash - *remaining_text;
      gint found = 0;
      gchar *found_name = NULL;         /* Quiet gcc */
      gint i;
      gchar* pat_buf = g_new (gchar, len + 1);

      strncpy (pat_buf, *remaining_text, len);
      pat_buf[len] = 0;

      for (i = 0; i < dir->sent->entry_count; i += 1)
	{
	  if (dir->sent->entries[i].is_dir &&
	      _gtk_fnmatch (pat_buf, dir->sent->entries[i].entry_name, TRUE))
	    {
	      if (found)
		{
		  g_free (pat_buf);
		  return dir;
		}
	      else
		{
		  found = 1;
		  found_name = dir->sent->entries[i].entry_name;
		}
	    }
	}

      if (!found)
	{
	  /* Perhaps we are trying to open an automount directory */
	  found_name = pat_buf;
	}

      next = open_relative_dir (found_name, dir, cmpl_state);
      
      if (!next)
	{
	  g_free (pat_buf);
	  return NULL;
}
      
      next->cmpl_parent = dir;
      
      dir = next;
      
      if (!correct_dir_fullname (dir))
	{
	  g_free (pat_buf);
	  return NULL;
	}
      
      *remaining_text = first_slash + 1;
      first_slash = strchr (*remaining_text, G_DIR_SEPARATOR);

      g_free (pat_buf);
    }

  return dir;
}

static void
update_cmpl (PossibleCompletion *poss,
	     CompletionState    *cmpl_state)
{
  gint cmpl_len;

  if (!poss || !cmpl_is_a_completion (poss))
    return;

  cmpl_len = strlen (cmpl_this_completion (poss));

  if (cmpl_state->updated_text_alloc < cmpl_len + 1)
    {
      cmpl_state->updated_text_alloc = 2*cmpl_len;
      cmpl_state->updated_text =
	(gchar*)g_realloc (cmpl_state->updated_text,
			   cmpl_state->updated_text_alloc);
    }

  if (cmpl_state->updated_text_len < 0)
    {
      strcpy (cmpl_state->updated_text, cmpl_this_completion (poss));
      cmpl_state->updated_text_len = cmpl_len;
      cmpl_state->re_complete = cmpl_is_directory (poss);
    }
  else if (cmpl_state->updated_text_len == 0)
    {
      cmpl_state->re_complete = FALSE;
    }
  else
    {
      gint first_diff =
	first_diff_index (cmpl_state->updated_text,
			  cmpl_this_completion (poss));

      cmpl_state->re_complete = FALSE;

      if (first_diff == PATTERN_MATCH)
	return;

      if (first_diff > cmpl_state->updated_text_len)
	strcpy (cmpl_state->updated_text, cmpl_this_completion (poss));

      cmpl_state->updated_text_len = first_diff;
      cmpl_state->updated_text[first_diff] = 0;
    }
}

static PossibleCompletion*
attempt_file_completion (CompletionState *cmpl_state)
{
  gchar *pat_buf, *first_slash;
  CompletionDir *dir = cmpl_state->active_completion_dir;

  dir->cmpl_index += 1;

  if (dir->cmpl_index == dir->sent->entry_count)
    {
      if (dir->cmpl_parent == NULL)
	{
	  cmpl_state->active_completion_dir = NULL;

	  return NULL;
	}
      else
	{
	  cmpl_state->active_completion_dir = dir->cmpl_parent;

	  return attempt_file_completion (cmpl_state);
	}
    }

  g_assert (dir->cmpl_text);

  first_slash = strchr (dir->cmpl_text, G_DIR_SEPARATOR);

  if (first_slash)
    {
      gint len = first_slash - dir->cmpl_text;

      pat_buf = g_new (gchar, len + 1);
      strncpy (pat_buf, dir->cmpl_text, len);
      pat_buf[len] = 0;
    }
  else
    {
      gint len = strlen (dir->cmpl_text);

      pat_buf = g_new (gchar, len + 2);
      strcpy (pat_buf, dir->cmpl_text);
      /* Don't append a * if the user entered one herself.
       * This way one can complete *.h and don't get matches
       * on any .help files, for instance.
       */
      if (strchr (pat_buf, '*') == NULL)
	strcpy (pat_buf + len, "*");
    }

  if (first_slash)
    {
      if (dir->sent->entries[dir->cmpl_index].is_dir)
	{
	  if (_gtk_fnmatch (pat_buf, dir->sent->entries[dir->cmpl_index].entry_name, TRUE))
	    {
	      CompletionDir* new_dir;

	      new_dir = open_relative_dir (dir->sent->entries[dir->cmpl_index].entry_name,
					   dir, cmpl_state);

	      if (!new_dir)
		{
		  g_free (pat_buf);
		  return NULL;
		}

	      new_dir->cmpl_parent = dir;

	      new_dir->cmpl_index = -1;
	      new_dir->cmpl_text = g_strdup (first_slash + 1);

	      cmpl_state->active_completion_dir = new_dir;

	      g_free (pat_buf);
	      return attempt_file_completion (cmpl_state);
	    }
	  else
	    {
	      g_free (pat_buf);
	      return attempt_file_completion (cmpl_state);
	    }
	}
      else
	{
	  g_free (pat_buf);
	  return attempt_file_completion (cmpl_state);
	}
    }
  else
    {
      if (dir->cmpl_parent != NULL)
	{
	  append_completion_text (dir->fullname +
				  strlen (cmpl_state->completion_dir->fullname) + 1,
				  cmpl_state);
	  append_completion_text (G_DIR_SEPARATOR_S, cmpl_state);
	}

      append_completion_text (dir->sent->entries[dir->cmpl_index].entry_name, cmpl_state);

      cmpl_state->the_completion.is_a_completion =
	_gtk_fnmatch (pat_buf, dir->sent->entries[dir->cmpl_index].entry_name, TRUE);

      cmpl_state->the_completion.is_directory = dir->sent->entries[dir->cmpl_index].is_dir;
      if (dir->sent->entries[dir->cmpl_index].is_dir)
	append_completion_text (G_DIR_SEPARATOR_S, cmpl_state);

      g_free (pat_buf);
      return &cmpl_state->the_completion;
    }
}

#ifdef HAVE_PWD_H

static gint
get_pwdb (CompletionState* cmpl_state)
{
  struct passwd *pwd_ptr;
  gchar* buf_ptr;
  gchar *utf8;
  gint len = 0, i, count = 0;

  if (cmpl_state->user_dir_name_buffer)
    return TRUE;
  setpwent ();

  while ((pwd_ptr = getpwent ()) != NULL)
    {
      utf8 = g_filename_to_utf8 (pwd_ptr->pw_name, -1, NULL, NULL, NULL);
      len += strlen (utf8);
      g_free (utf8);
      utf8 = g_filename_to_utf8 (pwd_ptr->pw_dir, -1, NULL, NULL, NULL);
      len += strlen (utf8);
      g_free (utf8);
      len += 2;
      count += 1;
    }

  setpwent ();

  cmpl_state->user_dir_name_buffer = g_new (gchar, len);
  cmpl_state->user_directories = g_new (CompletionUserDir, count);
  cmpl_state->user_directories_len = count;

  buf_ptr = cmpl_state->user_dir_name_buffer;

  for (i = 0; i < count; i += 1)
    {
      pwd_ptr = getpwent ();
      if (!pwd_ptr)
	{
	  cmpl_errno = errno;
	  goto error;
	}

      utf8 = g_filename_to_utf8 (pwd_ptr->pw_name, -1, NULL, NULL, NULL);
      strcpy (buf_ptr, utf8);
      g_free (utf8);

      cmpl_state->user_directories[i].login = buf_ptr;

      buf_ptr += strlen (buf_ptr);
      buf_ptr += 1;

      utf8 = g_filename_to_utf8 (pwd_ptr->pw_dir, -1, NULL, NULL, NULL);
      strcpy (buf_ptr, utf8);
      g_free (utf8);

      cmpl_state->user_directories[i].homedir = buf_ptr;

      buf_ptr += strlen (buf_ptr);
      buf_ptr += 1;
    }

  qsort (cmpl_state->user_directories,
	 cmpl_state->user_directories_len,
	 sizeof (CompletionUserDir),
	 compare_user_dir);

  endpwent ();

  return TRUE;

error:

  g_free (cmpl_state->user_dir_name_buffer);
  g_free (cmpl_state->user_directories);

  cmpl_state->user_dir_name_buffer = NULL;
  cmpl_state->user_directories = NULL;

  return FALSE;
}

static gint
compare_user_dir (const void *a,
		  const void *b)
{
  return strcmp ((((CompletionUserDir*)a))->login,
		 (((CompletionUserDir*)b))->login);
}

#endif

static gint
compare_cmpl_dir (const void *a,
		  const void *b)
{
  
  return strcmp (((CompletionDirEntry*)a)->sort_key,
		 (((CompletionDirEntry*)b))->sort_key);
}

static gint
cmpl_state_okay (CompletionState* cmpl_state)
{
  return  cmpl_state && cmpl_state->reference_dir;
}

static const gchar*
cmpl_strerror (gint err)
{
  if (err == CMPL_ERRNO_TOO_LONG)
    return _("Name too long");
  else if (err == CMPL_ERRNO_DID_NOT_CONVERT)
    return _("Couldn't convert filename");
  else
    return g_strerror (err);
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

/* DLL ABI stability backward compatibility versions */

#undef gtk_file_selection_get_filename

const gchar*
gtk_file_selection_get_filename (GtkFileSelection *filesel)
{
  static gchar retval[MAXPATHLEN*2+1];
  gchar *tem;

  tem = g_locale_from_utf8 (gtk_file_selection_get_filename_utf8 (filesel),
			    -1, NULL, NULL, NULL);

  strncpy (retval, tem, sizeof (retval) - 1);
  retval[sizeof (retval) - 1] = '\0';
  g_free (tem);

  return retval;
}

#undef gtk_file_selection_set_filename

void
gtk_file_selection_set_filename (GtkFileSelection *filesel,
				 const gchar      *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  gtk_file_selection_set_filename_utf8 (filesel, utf8_filename);
  g_free (utf8_filename);
}

#undef gtk_file_selection_get_selections

gchar **
gtk_file_selection_get_selections (GtkFileSelection *filesel)
{
  int i = 0;
  gchar **selections = gtk_file_selection_get_selections_utf8 (filesel);

  if (selections != NULL)
    while (selections[i] != NULL)
      {
	gchar *tem = selections[i];
	selections[i] = g_locale_from_utf8 (selections[i],
					    -1, NULL, NULL, NULL);
	g_free (tem);
	i++;
      }

  return selections;
}

#endif /* G_OS_WIN32 && !_WIN64 */

#define __GTK_FILESEL_C__
#include "gtkaliasdef.c"
