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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include "fnmatch.h"

#include "gdk/gdkkeysyms.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkfilesel.h"
#include "gtkhbox.h"
#include "gtkhbbox.h"
#include "gtklabel.h"
#include "gtklist.h"
#include "gtklistitem.h"
#include "gtkmain.h"
#include "gtkscrolledwindow.h"
#include "gtksignal.h"
#include "gtkvbox.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkoptionmenu.h"
#include "gtkclist.h"
#include "gtkdialog.h"
#include "gtkintl.h"

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
/* The arguments used by all fnmatch() calls below
 */
#define FNMATCH_FLAGS (FNM_PATHNAME | FNM_PERIOD)

#define CMPL_ERRNO_TOO_LONG ((1<<16)-1)

/* This structure contains all the useful information about a directory
 * for the purposes of filename completion.  These structures are cached
 * in the CompletionState struct.  CompletionDir's are reference counted.
 */
struct _CompletionDirSent
{
  ino_t inode;
  time_t mtime;
  dev_t device;

  gint entry_count;
  gchar *name_buffer; /* memory segment containing names of all entries */

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
  gint is_dir;
  gchar *entry_name;
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
  gint is_directory;

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
  gint re_complete;

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


/* File completion functions which would be external, were they used
 * outside of this file.
 */

static CompletionState*    cmpl_init_state        (void);
static void                cmpl_free_state        (CompletionState *cmpl_state);
static gint                cmpl_state_okay        (CompletionState* cmpl_state);
static gchar*              cmpl_strerror          (gint);

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
static gint                cmpl_is_directory      (PossibleCompletion*);

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
static gint                cmpl_updated_dir        (CompletionState* cmpl_state);

/* Current location: if using file completion, return the current
 * directory, from which file completion begins.  More specifically,
 * the cwd concatenated with all exact completions up to the last
 * directory delimiter('/').
 */
static gchar*              cmpl_reference_position (CompletionState* cmpl_state);

/* backing up: if cmpl_completion_matches returns NULL, you may query
 * the index of the last completable character into cmpl_updated_text.
 */
static gint                cmpl_last_valid_char    (CompletionState* cmpl_state);

/* When the user selects a non-directory, call cmpl_completion_fullname
 * to get the full name of the selected file.
 */
static gchar*              cmpl_completion_fullname (gchar*, CompletionState* cmpl_state);


/* Directory operations. */
static CompletionDir* open_ref_dir         (gchar* text_to_complete,
					    gchar** remaining_text,
					    CompletionState* cmpl_state);
static gboolean       check_dir            (gchar *dir_name, 
					    struct stat *result, 
					    gboolean *stat_subdirs);
static CompletionDir* open_dir             (gchar* dir_name,
					    CompletionState* cmpl_state);
static CompletionDir* open_user_dir        (gchar* text_to_complete,
					    CompletionState *cmpl_state);
static CompletionDir* open_relative_dir    (gchar* dir_name, CompletionDir* dir,
					    CompletionState *cmpl_state);
static CompletionDirSent* open_new_dir     (gchar* dir_name, 
					    struct stat* sbuf,
					    gboolean stat_subdirs);
static gint           correct_dir_fullname (CompletionDir* cmpl_dir);
static gint           correct_parent       (CompletionDir* cmpl_dir,
					    struct stat *sbuf);
static gchar*         find_parent_dir_fullname    (gchar* dirname);
static CompletionDir* attach_dir           (CompletionDirSent* sent,
					    gchar* dir_name,
					    CompletionState *cmpl_state);
static void           free_dir_sent (CompletionDirSent* sent);
static void           free_dir      (CompletionDir  *dir);
static void           prune_memory_usage(CompletionState *cmpl_state);

/* Completion operations */
static PossibleCompletion* attempt_homedir_completion(gchar* text_to_complete,
						      CompletionState *cmpl_state);
static PossibleCompletion* attempt_file_completion(CompletionState *cmpl_state);
static CompletionDir* find_completion_dir(gchar* text_to_complete,
					  gchar** remaining_text,
					  CompletionState* cmpl_state);
static PossibleCompletion* append_completion_text(gchar* text,
						  CompletionState* cmpl_state);
static gint get_pwdb(CompletionState* cmpl_state);
static gint first_diff_index(gchar* pat, gchar* text);
static gint compare_user_dir(const void* a, const void* b);
static gint compare_cmpl_dir(const void* a, const void* b);
static void update_cmpl(PossibleCompletion* poss,
			CompletionState* cmpl_state);

static void gtk_file_selection_class_init    (GtkFileSelectionClass *klass);
static void gtk_file_selection_init          (GtkFileSelection      *filesel);
static void gtk_file_selection_destroy       (GtkObject             *object);
static gint gtk_file_selection_key_press     (GtkWidget             *widget,
					      GdkEventKey           *event,
					      gpointer               user_data);

static void gtk_file_selection_file_button (GtkWidget *widget,
					    gint row, 
					    gint column, 
					    GdkEventButton *bevent,
					    gpointer user_data);

static void gtk_file_selection_dir_button (GtkWidget *widget,
					   gint row, 
					   gint column, 
					   GdkEventButton *bevent,
					   gpointer data);

static void gtk_file_selection_populate      (GtkFileSelection      *fs,
					      gchar                 *rel_path,
					      gint                   try_complete);
static void gtk_file_selection_abort         (GtkFileSelection      *fs);

static void gtk_file_selection_update_history_menu (GtkFileSelection       *fs,
						    gchar                  *current_dir);

static void gtk_file_selection_create_dir (GtkWidget *widget, gpointer data);
static void gtk_file_selection_delete_file (GtkWidget *widget, gpointer data);
static void gtk_file_selection_rename_file (GtkWidget *widget, gpointer data);



static GtkWindowClass *parent_class = NULL;

/* Saves errno when something cmpl does fails. */
static gint cmpl_errno;

GtkType
gtk_file_selection_get_type (void)
{
  static GtkType file_selection_type = 0;

  if (!file_selection_type)
    {
      static const GtkTypeInfo filesel_info =
      {
	"GtkFileSelection",
	sizeof (GtkFileSelection),
	sizeof (GtkFileSelectionClass),
	(GtkClassInitFunc) gtk_file_selection_class_init,
	(GtkObjectInitFunc) gtk_file_selection_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      file_selection_type = gtk_type_unique (GTK_TYPE_WINDOW, &filesel_info);
    }

  return file_selection_type;
}

static void
gtk_file_selection_class_init (GtkFileSelectionClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_WINDOW);

  object_class->destroy = gtk_file_selection_destroy;
}

static void
gtk_file_selection_init (GtkFileSelection *filesel)
{
  GtkWidget *entry_vbox;
  GtkWidget *label;
  GtkWidget *list_hbox;
  GtkWidget *confirm_area;
  GtkWidget *pulldown_hbox;
  GtkWidget *scrolled_win;

  char *dir_title [2];
  char *file_title [2];
  
  filesel->cmpl_state = cmpl_init_state ();

  /* The dialog-sized vertical box  */
  filesel->main_vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (filesel), 10);
  gtk_container_add (GTK_CONTAINER (filesel), filesel->main_vbox);
  gtk_widget_show (filesel->main_vbox);

  /* The horizontal box containing create, rename etc. buttons */
  filesel->button_area = gtk_hbutton_box_new ();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(filesel->button_area), GTK_BUTTONBOX_START);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(filesel->button_area), 0);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), filesel->button_area, 
		      FALSE, FALSE, 0);
  gtk_widget_show (filesel->button_area);
  
  gtk_file_selection_show_fileop_buttons(filesel);

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
  list_hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), list_hbox, TRUE, TRUE, 0);
  gtk_widget_show (list_hbox);

  /* The directories clist */
  dir_title[0] = _("Directories");
  dir_title[1] = NULL;
  filesel->dir_list = gtk_clist_new_with_titles (1, (gchar**) dir_title);
  gtk_widget_set_usize (filesel->dir_list, DIR_LIST_WIDTH, DIR_LIST_HEIGHT);
  gtk_signal_connect (GTK_OBJECT (filesel->dir_list), "select_row",
		      (GtkSignalFunc) gtk_file_selection_dir_button, 
		      (gpointer) filesel);
  gtk_clist_column_titles_passive (GTK_CLIST (filesel->dir_list));

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_win), filesel->dir_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_show (filesel->dir_list);
  gtk_widget_show (scrolled_win);

  /* The files clist */
  file_title[0] = _("Files");
  file_title[1] = NULL;
  filesel->file_list = gtk_clist_new_with_titles (1, (gchar**) file_title);
  gtk_widget_set_usize (filesel->file_list, FILE_LIST_WIDTH, FILE_LIST_HEIGHT);
  gtk_signal_connect (GTK_OBJECT (filesel->file_list), "select_row",
		      (GtkSignalFunc) gtk_file_selection_file_button, 
		      (gpointer) filesel);
  gtk_clist_column_titles_passive (GTK_CLIST (filesel->file_list));

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_win), filesel->file_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_show (filesel->file_list);
  gtk_widget_show (scrolled_win);

  /* action area for packing buttons into. */
  filesel->action_area = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (filesel->main_vbox), filesel->action_area, 
		      FALSE, FALSE, 0);
  gtk_widget_show (filesel->action_area);
  
  /*  The OK/Cancel button area */
  confirm_area = gtk_hbutton_box_new ();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(confirm_area), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(confirm_area), 5);
  gtk_box_pack_end (GTK_BOX (filesel->main_vbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_show (confirm_area);

  /*  The OK button  */
  filesel->ok_button = gtk_button_new_with_label (_("OK"));
  GTK_WIDGET_SET_FLAGS (filesel->ok_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (confirm_area), filesel->ok_button, TRUE, TRUE, 0);
  gtk_widget_grab_default (filesel->ok_button);
  gtk_widget_show (filesel->ok_button);

  /*  The Cancel button  */
  filesel->cancel_button = gtk_button_new_with_label (_("Cancel"));
  GTK_WIDGET_SET_FLAGS (filesel->cancel_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (confirm_area), filesel->cancel_button, TRUE, TRUE, 0);
  gtk_widget_show (filesel->cancel_button);

  /*  The selection entry widget  */
  entry_vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_end (GTK_BOX (filesel->main_vbox), entry_vbox, FALSE, FALSE, 0);
  gtk_widget_show (entry_vbox);

  filesel->selection_text = label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (entry_vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  filesel->selection_entry = gtk_entry_new ();
  gtk_signal_connect (GTK_OBJECT (filesel->selection_entry), "key_press_event",
		      (GtkSignalFunc) gtk_file_selection_key_press, filesel);
  gtk_signal_connect_object (GTK_OBJECT (filesel->selection_entry), "focus_in_event",
			     (GtkSignalFunc) gtk_widget_grab_default,
			     GTK_OBJECT (filesel->ok_button));
  gtk_signal_connect_object (GTK_OBJECT (filesel->selection_entry), "activate",
                             (GtkSignalFunc) gtk_button_clicked,
                             GTK_OBJECT (filesel->ok_button));
  gtk_box_pack_start (GTK_BOX (entry_vbox), filesel->selection_entry, TRUE, TRUE, 0);
  gtk_widget_show (filesel->selection_entry);

  if (!cmpl_state_okay (filesel->cmpl_state))
    {
      gchar err_buf[256];

      sprintf (err_buf, _("Directory unreadable: %s"), cmpl_strerror (cmpl_errno));

      gtk_label_set_text (GTK_LABEL (filesel->selection_text), err_buf);
    }
  else
    {
      gtk_file_selection_populate (filesel, "", FALSE);
    }

  gtk_widget_grab_focus (filesel->selection_entry);
}

GtkWidget*
gtk_file_selection_new (const gchar *title)
{
  GtkFileSelection *filesel;

  filesel = gtk_type_new (GTK_TYPE_FILE_SELECTION);
  gtk_window_set_title (GTK_WINDOW (filesel), title);

  return GTK_WIDGET (filesel);
}

void
gtk_file_selection_show_fileop_buttons (GtkFileSelection *filesel)
{
  g_return_if_fail (filesel != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
    
  /* delete, create directory, and rename */
  if (!filesel->fileop_c_dir) 
    {
      filesel->fileop_c_dir = gtk_button_new_with_label (_("Create Dir"));
      gtk_signal_connect (GTK_OBJECT (filesel->fileop_c_dir), "clicked",
			  (GtkSignalFunc) gtk_file_selection_create_dir, 
			  (gpointer) filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_c_dir, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_c_dir);
    }
	
  if (!filesel->fileop_del_file) 
    {
      filesel->fileop_del_file = gtk_button_new_with_label (_("Delete File"));
      gtk_signal_connect (GTK_OBJECT (filesel->fileop_del_file), "clicked",
			  (GtkSignalFunc) gtk_file_selection_delete_file, 
			  (gpointer) filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_del_file, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_del_file);
    }

  if (!filesel->fileop_ren_file)
    {
      filesel->fileop_ren_file = gtk_button_new_with_label (_("Rename File"));
      gtk_signal_connect (GTK_OBJECT (filesel->fileop_ren_file), "clicked",
			  (GtkSignalFunc) gtk_file_selection_rename_file, 
			  (gpointer) filesel);
      gtk_box_pack_start (GTK_BOX (filesel->button_area), 
			  filesel->fileop_ren_file, TRUE, TRUE, 0);
      gtk_widget_show (filesel->fileop_ren_file);
    }

  gtk_widget_queue_resize(GTK_WIDGET(filesel));
}

void       
gtk_file_selection_hide_fileop_buttons (GtkFileSelection *filesel)
{
  g_return_if_fail (filesel != NULL);
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
}



void
gtk_file_selection_set_filename (GtkFileSelection *filesel,
				 const gchar      *filename)
{
  char  buf[MAXPATHLEN];
  const char *name, *last_slash;

  g_return_if_fail (filesel != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
  g_return_if_fail (filename != NULL);

  last_slash = strrchr (filename, '/');

  if (!last_slash)
    {
      buf[0] = 0;
      name = filename;
    }
  else
    {
      gint len = MIN (MAXPATHLEN - 1, last_slash - filename + 1);

      strncpy (buf, filename, len);
      buf[len] = 0;

      name = last_slash + 1;
    }

  gtk_file_selection_populate (filesel, buf, FALSE);

  if (filesel->selection_entry)
    gtk_entry_set_text (GTK_ENTRY (filesel->selection_entry), name);
}

gchar*
gtk_file_selection_get_filename (GtkFileSelection *filesel)
{
  static char nothing[2] = "";
  char *text;
  char *filename;

  g_return_val_if_fail (filesel != NULL, nothing);
  g_return_val_if_fail (GTK_IS_FILE_SELECTION (filesel), nothing);

  text = gtk_entry_get_text (GTK_ENTRY (filesel->selection_entry));
  if (text)
    {
      filename = cmpl_completion_fullname (text, filesel->cmpl_state);
      return filename;
    }

  return nothing;
}

void
gtk_file_selection_complete (GtkFileSelection *filesel,
			     const gchar      *pattern)
{
  g_return_if_fail (filesel != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (filesel));
  g_return_if_fail (pattern != NULL);

  if (filesel->selection_entry)
    gtk_entry_set_text (GTK_ENTRY (filesel->selection_entry), pattern);
  gtk_file_selection_populate (filesel, (gchar*) pattern, TRUE);
}

static void
gtk_file_selection_destroy (GtkObject *object)
{
  GtkFileSelection *filesel;
  GList *list;
  HistoryCallbackArg *callback_arg;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (object));

  filesel = GTK_FILE_SELECTION (object);
  
  if (filesel->fileop_dialog)
	  gtk_widget_destroy (filesel->fileop_dialog);
  
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
  
  cmpl_free_state (filesel->cmpl_state);
  filesel->cmpl_state = NULL;

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Begin file operations callbacks */

static void
gtk_file_selection_fileop_error (GtkFileSelection *fs, gchar *error_message)
{
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *dialog;
  
  g_return_if_fail (error_message != NULL);
  
  /* main dialog */
  dialog = gtk_dialog_new ();
  /*
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      (GtkSignalFunc) gtk_file_selection_fileop_destroy, 
		      (gpointer) fs);
  */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  
  /* If file dialog is grabbed, make this dialog modal too */
  /* When error dialog is closed, file dialog will be grabbed again */
  if (GTK_WINDOW(fs)->modal)
      gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
		     FALSE, FALSE, 0);
  gtk_widget_show(vbox);

  label = gtk_label_new(error_message);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);

  /* yes, we free it */
  g_free (error_message);
  
  /* close button */
  button = gtk_button_new_with_label (_("Close"));
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy, 
			     (gpointer) dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(button);
  gtk_widget_show (button);

  gtk_widget_show (dialog);
}

static void
gtk_file_selection_fileop_destroy (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;

  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));
  
  fs->fileop_dialog = NULL;
}


static void
gtk_file_selection_create_dir_confirmed (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  gchar *dirname;
  gchar *path;
  gchar *full_path;
  gchar *buf;
  CompletionState *cmpl_state;
  
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  dirname = gtk_entry_get_text (GTK_ENTRY (fs->fileop_entry));
  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  full_path = g_strconcat (path, "/", dirname, NULL);
  if ( (mkdir (full_path, 0755) < 0) ) 
    {
      buf = g_strconcat ("Error creating directory \"", dirname, "\":  ", 
			 g_strerror(errno), NULL);
      gtk_file_selection_fileop_error (fs, buf);
    }
  g_free (full_path);
  
  gtk_widget_destroy (fs->fileop_dialog);
  gtk_file_selection_populate (fs, "", FALSE);
}
  
static void
gtk_file_selection_create_dir (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  GtkWidget *label;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *button;

  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
	  return;
  
  /* main dialog */
  fs->fileop_dialog = dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      (GtkSignalFunc) gtk_file_selection_fileop_destroy, 
		      (gpointer) fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Create Directory"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

  /* If file dialog is grabbed, grab option dialog */
  /* When option dialog is closed, file dialog will be grabbed again */
  if (GTK_WINDOW(fs)->modal)
      gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
		     FALSE, FALSE, 0);
  gtk_widget_show(vbox);
  
  label = gtk_label_new(_("Directory name:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);

  /*  The directory entry widget  */
  fs->fileop_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), fs->fileop_entry, 
		      TRUE, TRUE, 5);
  GTK_WIDGET_SET_FLAGS(fs->fileop_entry, GTK_CAN_DEFAULT);
  gtk_widget_show (fs->fileop_entry);
  
  /* buttons */
  button = gtk_button_new_with_label (_("Create"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) gtk_file_selection_create_dir_confirmed, 
		      (gpointer) fs);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_show(button);
  
  button = gtk_button_new_with_label (_("Cancel"));
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy, 
			     (gpointer) dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(button);
  gtk_widget_show (button);

  gtk_widget_show (dialog);
}

static void
gtk_file_selection_delete_file_confirmed (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  CompletionState *cmpl_state;
  gchar *path;
  gchar *full_path;
  gchar *buf;
  
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  full_path = g_strconcat (path, "/", fs->fileop_file, NULL);
  if ( (unlink (full_path) < 0) ) 
    {
      buf = g_strconcat ("Error deleting file \"", fs->fileop_file, "\":  ", 
			 g_strerror(errno), NULL);
      gtk_file_selection_fileop_error (fs, buf);
    }
  g_free (full_path);
  
  gtk_widget_destroy (fs->fileop_dialog);
  gtk_file_selection_populate (fs, "", FALSE);
}

static void
gtk_file_selection_delete_file (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *dialog;
  gchar *filename;
  gchar *buf;
  
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
	  return;

  filename = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));
  if (strlen(filename) < 1)
	  return;

  fs->fileop_file = filename;
  
  /* main dialog */
  fs->fileop_dialog = dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      (GtkSignalFunc) gtk_file_selection_fileop_destroy, 
		      (gpointer) fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete File"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

  /* If file dialog is grabbed, grab option dialog */
  /* When option dialog is closed, file dialog will be grabbed again */
  if (GTK_WINDOW(fs)->modal)
      gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
  
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
		     FALSE, FALSE, 0);
  gtk_widget_show(vbox);

  buf = g_strconcat ("Really delete file \"", filename, "\" ?", NULL);
  label = gtk_label_new(buf);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  g_free(buf);
  
  /* buttons */
  button = gtk_button_new_with_label (_("Delete"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) gtk_file_selection_delete_file_confirmed, 
		      (gpointer) fs);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_show(button);
  
  button = gtk_button_new_with_label (_("Cancel"));
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy, 
			     (gpointer) dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(button);
  gtk_widget_show (button);

  gtk_widget_show (dialog);

}

static void
gtk_file_selection_rename_file_confirmed (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  gchar *buf;
  gchar *file;
  gchar *path;
  gchar *new_filename;
  gchar *old_filename;
  CompletionState *cmpl_state;
  
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  file = gtk_entry_get_text (GTK_ENTRY (fs->fileop_entry));
  cmpl_state = (CompletionState*) fs->cmpl_state;
  path = cmpl_reference_position (cmpl_state);
  
  new_filename = g_strconcat (path, "/", file, NULL);
  old_filename = g_strconcat (path, "/", fs->fileop_file, NULL);

  if ( (rename (old_filename, new_filename)) < 0) 
    {
      buf = g_strconcat ("Error renaming file \"", file, "\":  ", 
			 g_strerror(errno), NULL);
      gtk_file_selection_fileop_error (fs, buf);
    }
  g_free (new_filename);
  g_free (old_filename);
  
  gtk_widget_destroy (fs->fileop_dialog);
  gtk_file_selection_populate (fs, "", FALSE);
}
  
static void
gtk_file_selection_rename_file (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  GtkWidget *label;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *button;
  gchar *buf;
  
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  if (fs->fileop_dialog)
	  return;

  fs->fileop_file = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));
  if (strlen(fs->fileop_file) < 1)
	  return;
  
  /* main dialog */
  fs->fileop_dialog = dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      (GtkSignalFunc) gtk_file_selection_fileop_destroy, 
		      (gpointer) fs);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Rename File"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

  /* If file dialog is grabbed, grab option dialog */
  /* When option dialog  closed, file dialog will be grabbed again */
  if (GTK_WINDOW(fs)->modal)
    gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
  
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
		     FALSE, FALSE, 0);
  gtk_widget_show(vbox);
  
  buf = g_strconcat ("Rename file \"", fs->fileop_file, "\" to:", NULL);
  label = gtk_label_new(buf);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  g_free(buf);

  /* New filename entry */
  fs->fileop_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), fs->fileop_entry, 
		      TRUE, TRUE, 5);
  GTK_WIDGET_SET_FLAGS(fs->fileop_entry, GTK_CAN_DEFAULT);
  gtk_widget_show (fs->fileop_entry);
  
  gtk_entry_set_text (GTK_ENTRY (fs->fileop_entry), fs->fileop_file);
  gtk_editable_select_region (GTK_EDITABLE (fs->fileop_entry),
			      0, strlen (fs->fileop_file));

  /* buttons */
  button = gtk_button_new_with_label (_("Rename"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) gtk_file_selection_rename_file_confirmed, 
		      (gpointer) fs);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_show(button);
  
  button = gtk_button_new_with_label (_("Cancel"));
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy, 
			     (gpointer) dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(button);
  gtk_widget_show (button);

  gtk_widget_show (dialog);
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

  if (event->keyval == GDK_Tab)
    {
      fs = GTK_FILE_SELECTION (user_data);
      text = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));

      text = g_strdup (text);

      gtk_file_selection_populate (fs, text, TRUE);

      g_free (text);

      gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");

      return TRUE;
    }

  return FALSE;
}


static void
gtk_file_selection_history_callback (GtkWidget *widget, gpointer data)
{
  GtkFileSelection *fs = data;
  HistoryCallbackArg *callback_arg;
  GList *list;

  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  list = fs->history_list;
  
  while (list) {
    callback_arg = list->data;
    
    if (callback_arg->menu_item == widget)
      {
	gtk_file_selection_populate (fs, callback_arg->directory, FALSE);
	break;
      }
    
    list = list->next;
  }
}

static void 
gtk_file_selection_update_history_menu (GtkFileSelection *fs,
					gchar *current_directory)
{
  HistoryCallbackArg *callback_arg;
  GtkWidget *menu_item;
  GList *list;
  gchar *current_dir;
  gint dir_len;
  gint i;
  
  g_return_if_fail (fs != NULL);
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
  
  fs->history_menu = gtk_menu_new();

  current_dir = g_strdup (current_directory);

  dir_len = strlen (current_dir);

  for (i = dir_len; i >= 0; i--)
    {
      /* the i == dir_len is to catch the full path for the first 
       * entry. */
      if ( (current_dir[i] == '/') || (i == dir_len))
	{
	  /* another small hack to catch the full path */
	  if (i != dir_len) 
		  current_dir[i + 1] = '\0';
	  menu_item = gtk_menu_item_new_with_label (current_dir);
	  
	  callback_arg = g_new (HistoryCallbackArg, 1);
	  callback_arg->menu_item = menu_item;
	  
	  /* since the autocompletion gets confused if you don't 
	   * supply a trailing '/' on a dir entry, set the full
	   * (current) path to "" which just refreshes the filesel */
	  if (dir_len == i) {
	    callback_arg->directory = g_strdup ("");
	  } else {
	    callback_arg->directory = g_strdup (current_dir);
	  }
	  
	  fs->history_list = g_list_append (fs->history_list, callback_arg);
	  
	  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			      (GtkSignalFunc) gtk_file_selection_history_callback,
			      (gpointer) fs);
	  gtk_menu_append (GTK_MENU (fs->history_menu), menu_item);
	  gtk_widget_show (menu_item);
	}
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (fs->history_pulldown), 
			    fs->history_menu);
  g_free (current_dir);
}

static void
gtk_file_selection_file_button (GtkWidget *widget,
			       gint row, 
			       gint column, 
			       GdkEventButton *bevent,
			       gpointer user_data)
{
  GtkFileSelection *fs = NULL;
  gchar *filename, *temp = NULL;
  
  g_return_if_fail (GTK_IS_CLIST (widget));

  fs = user_data;
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));
  
  gtk_clist_get_text (GTK_CLIST (fs->file_list), row, 0, &temp);
  filename = g_strdup (temp);

  if (filename)
    {
      if (bevent)
	switch (bevent->type)
	  {
	  case GDK_2BUTTON_PRESS:
	    gtk_button_clicked (GTK_BUTTON (fs->ok_button));
	    break;
	    
	  default:
	    gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);
	    break;
	  }
      else
	gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);

      g_free (filename);
    }
}

static void
gtk_file_selection_dir_button (GtkWidget *widget,
			       gint row, 
			       gint column, 
			       GdkEventButton *bevent,
			       gpointer user_data)
{
  GtkFileSelection *fs = NULL;
  gchar *filename, *temp = NULL;

  g_return_if_fail (GTK_IS_CLIST (widget));

  fs = GTK_FILE_SELECTION (user_data);
  g_return_if_fail (fs != NULL);
  g_return_if_fail (GTK_IS_FILE_SELECTION (fs));

  gtk_clist_get_text (GTK_CLIST (fs->dir_list), row, 0, &temp);
  filename = g_strdup (temp);

  if (filename)
    {
      if (bevent)
	switch (bevent->type)
	  {
	  case GDK_2BUTTON_PRESS:
	    gtk_file_selection_populate (fs, filename, FALSE);
	    break;
	  
	  default:
	    gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);
	    break;
	  }
      else
	gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), filename);

      g_free (filename);
    }
}

static void
gtk_file_selection_populate (GtkFileSelection *fs,
			     gchar            *rel_path,
			     gint              try_complete)
{
  CompletionState *cmpl_state;
  PossibleCompletion* poss;
  gchar* filename;
  gint row;
  gchar* rem_path = rel_path;
  gchar* sel_text;
  gchar* text[2];
  gint did_recurse = FALSE;
  gint possible_count = 0;
  gint selection_index = -1;
  gint file_list_width;
  gint dir_list_width;
  
  g_return_if_fail (fs != NULL);
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

  gtk_clist_freeze (GTK_CLIST (fs->dir_list));
  gtk_clist_clear (GTK_CLIST (fs->dir_list));
  gtk_clist_freeze (GTK_CLIST (fs->file_list));
  gtk_clist_clear (GTK_CLIST (fs->file_list));

  /* Set the dir_list to include ./ and ../ */
  text[1] = NULL;
  text[0] = "./";
  row = gtk_clist_append (GTK_CLIST (fs->dir_list), text);

  text[0] = "../";
  row = gtk_clist_append (GTK_CLIST (fs->dir_list), text);

  /*reset the max widths of the lists*/
  dir_list_width = gdk_string_width(fs->dir_list->style->font,"../");
  gtk_clist_set_column_width(GTK_CLIST(fs->dir_list),0,dir_list_width);
  file_list_width = 1;
  gtk_clist_set_column_width(GTK_CLIST(fs->file_list),0,file_list_width);

  while (poss)
    {
      if (cmpl_is_a_completion (poss))
        {
          possible_count += 1;

          filename = cmpl_this_completion (poss);

	  text[0] = filename;
	  
          if (cmpl_is_directory (poss))
            {
              if (strcmp (filename, "./") != 0 &&
                  strcmp (filename, "../") != 0)
		{
		  int width = gdk_string_width(fs->dir_list->style->font,
					       filename);
		  row = gtk_clist_append (GTK_CLIST (fs->dir_list), text);
		  if(width > dir_list_width)
		    {
		      dir_list_width = width;
		      gtk_clist_set_column_width(GTK_CLIST(fs->dir_list),0,
						 width);
		    }
 		}
	    }
          else
	    {
	      int width = gdk_string_width(fs->file_list->style->font,
				           filename);
	      row = gtk_clist_append (GTK_CLIST (fs->file_list), text);
	      if(width > file_list_width)
	        {
	          file_list_width = width;
	          gtk_clist_set_column_width(GTK_CLIST(fs->file_list),0,
					     width);
	        }
            }
	}

      poss = cmpl_next_completion (cmpl_state);
    }

  gtk_clist_thaw (GTK_CLIST (fs->dir_list));
  gtk_clist_thaw (GTK_CLIST (fs->file_list));

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

              gtk_file_selection_populate (fs, dir_name, TRUE);

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
          selection_index = cmpl_last_valid_char (cmpl_state) -
                            (strlen (rel_path) - strlen (rem_path));
	  if (fs->selection_entry)
	    gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), rem_path);
        }
    }
  else
    {
      if (fs->selection_entry)
	gtk_entry_set_text (GTK_ENTRY (fs->selection_entry), "");
    }

  if (!did_recurse)
    {
      if (fs->selection_entry)
	gtk_entry_set_position (GTK_ENTRY (fs->selection_entry), selection_index);

      if (fs->selection_entry)
	{
	  sel_text = g_strconcat (_("Selection: "),
				  cmpl_reference_position (cmpl_state),
				  NULL);

	  gtk_label_set_text (GTK_LABEL (fs->selection_text), sel_text);
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

  sprintf (err_buf, _("Directory unreadable: %s"), cmpl_strerror (cmpl_errno));

  /*  BEEP gdk_beep();  */

  if (fs->selection_entry)
    gtk_label_set_text (GTK_LABEL (fs->selection_text), err_buf);
}

/**********************************************************************/
/*			  External Interface                          */
/**********************************************************************/

/* The four completion state selectors
 */
static gchar*
cmpl_updated_text (CompletionState* cmpl_state)
{
  return cmpl_state->updated_text;
}

static gint
cmpl_updated_dir (CompletionState* cmpl_state)
{
  return cmpl_state->re_complete;
}

static gchar*
cmpl_reference_position (CompletionState* cmpl_state)
{
  return cmpl_state->reference_dir->fullname;
}

static gint
cmpl_last_valid_char (CompletionState* cmpl_state)
{
  return cmpl_state->last_valid_char;
}

static gchar*
cmpl_completion_fullname (gchar* text, CompletionState* cmpl_state)
{
  static char nothing[2] = "";

  if (!cmpl_state_okay (cmpl_state))
    {
      return nothing;
    }
  else if (text[0] == '/')
    {
      strcpy (cmpl_state->updated_text, text);
    }
  else if (text[0] == '~')
    {
      CompletionDir* dir;
      char* slash;

      dir = open_user_dir (text, cmpl_state);

      if (!dir)
	{
	  /* spencer says just return ~something, so
	   * for now just do it. */
	  strcpy (cmpl_state->updated_text, text);
	}
      else
	{

	  strcpy (cmpl_state->updated_text, dir->fullname);

	  slash = strchr (text, '/');

	  if (slash)
	    strcat (cmpl_state->updated_text, slash);
	}
    }
  else
    {
      strcpy (cmpl_state->updated_text, cmpl_state->reference_dir->fullname);
      if (strcmp (cmpl_state->reference_dir->fullname, "/") != 0)
	strcat (cmpl_state->updated_text, "/");
      strcat (cmpl_state->updated_text, text);
    }

  return cmpl_state->updated_text;
}

/* The three completion selectors
 */
static gchar*
cmpl_this_completion (PossibleCompletion* pc)
{
  return pc->text;
}

static gint
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

static CompletionState*
cmpl_init_state (void)
{
  gchar getcwd_buf[2*MAXPATHLEN];
  CompletionState *new_state;

  new_state = g_new (CompletionState, 1);

  /* We don't use getcwd() on SUNOS, because, it does a popen("pwd")
   * and, if that wasn't bad enough, hangs in doing so.
   */
#if defined(sun) && !defined(__SVR4)
  if (!getwd (getcwd_buf))
#else    
  if (!getcwd (getcwd_buf, MAXPATHLEN))
#endif    
    {
      /* Oh joy, we can't get the current directory. Um..., we should have
       * a root directory, right? Right? (Probably not portable to non-Unix)
       */
      strcpy (getcwd_buf, "/");
    }

tryagain:

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

  new_state->reference_dir =  open_dir (getcwd_buf, new_state);

  if (!new_state->reference_dir)
    {
      /* Directories changing from underneath us, grumble */
      strcpy (getcwd_buf, "/");
      goto tryagain;
    }

  return new_state;
}

static void
cmpl_free_dir_list(GList* dp0)
{
  GList *dp = dp0;

  while (dp) {
    free_dir (dp->data);
    dp = dp->next;
  }

  g_list_free(dp0);
}

static void
cmpl_free_dir_sent_list(GList* dp0)
{
  GList *dp = dp0;

  while (dp) {
    free_dir_sent (dp->data);
    dp = dp->next;
  }

  g_list_free(dp0);
}

static void
cmpl_free_state (CompletionState* cmpl_state)
{
  cmpl_free_dir_list (cmpl_state->directory_storage);
  cmpl_free_dir_sent_list (cmpl_state->directory_sent_storage);

  if (cmpl_state->user_dir_name_buffer)
    g_free (cmpl_state->user_dir_name_buffer);
  if (cmpl_state->user_directories)
    g_free (cmpl_state->user_directories);
  if (cmpl_state->the_completion.text)
    g_free (cmpl_state->the_completion.text);
  if (cmpl_state->updated_text)
    g_free (cmpl_state->updated_text);

  g_free (cmpl_state);
}

static void
free_dir(CompletionDir* dir)
{
  g_free(dir->fullname);
  g_free(dir);
}

static void
free_dir_sent(CompletionDirSent* sent)
{
  g_free(sent->name_buffer);
  g_free(sent->entries);
  g_free(sent);
}

static void
prune_memory_usage(CompletionState *cmpl_state)
{
  GList* cdsl = cmpl_state->directory_sent_storage;
  GList* cdl = cmpl_state->directory_storage;
  GList* cdl0 = cdl;
  gint len = 0;

  for(; cdsl && len < CMPL_DIRECTORY_CACHE_SIZE; len += 1)
    cdsl = cdsl->next;

  if (cdsl) {
    cmpl_free_dir_sent_list(cdsl->next);
    cdsl->next = NULL;
  }

  cmpl_state->directory_storage = NULL;
  while (cdl) {
    if (cdl->data == cmpl_state->reference_dir)
      cmpl_state->directory_storage = g_list_prepend(NULL, cdl->data);
    else
      free_dir (cdl->data);
    cdl = cdl->next;
  }

  g_list_free(cdl0);
}

/**********************************************************************/
/*                        The main entrances.                         */
/**********************************************************************/

static PossibleCompletion*
cmpl_completion_matches (gchar* text_to_complete,
			 gchar** remaining_text,
			 CompletionState* cmpl_state)
{
  gchar* first_slash;
  PossibleCompletion *poss;

  prune_memory_usage(cmpl_state);

  g_assert (text_to_complete != NULL);

  cmpl_state->user_completion_index = -1;
  cmpl_state->last_completion_text = text_to_complete;
  cmpl_state->the_completion.text[0] = 0;
  cmpl_state->last_valid_char = 0;
  cmpl_state->updated_text_len = -1;
  cmpl_state->updated_text[0] = 0;
  cmpl_state->re_complete = FALSE;

  first_slash = strchr (text_to_complete, '/');

  if (text_to_complete[0] == '~' && !first_slash)
    {
      /* Text starts with ~ and there is no slash, show all the
       * home directory completions.
       */
      poss = attempt_homedir_completion (text_to_complete, cmpl_state);

      update_cmpl(poss, cmpl_state);

      return poss;
    }

  cmpl_state->reference_dir =
    open_ref_dir (text_to_complete, remaining_text, cmpl_state);

  if(!cmpl_state->reference_dir)
    return NULL;

  cmpl_state->completion_dir =
    find_completion_dir (*remaining_text, remaining_text, cmpl_state);

  cmpl_state->last_valid_char = *remaining_text - text_to_complete;

  if(!cmpl_state->completion_dir)
    return NULL;

  cmpl_state->completion_dir->cmpl_index = -1;
  cmpl_state->completion_dir->cmpl_parent = NULL;
  cmpl_state->completion_dir->cmpl_text = *remaining_text;

  cmpl_state->active_completion_dir = cmpl_state->completion_dir;

  cmpl_state->reference_dir = cmpl_state->completion_dir;

  poss = attempt_file_completion(cmpl_state);

  update_cmpl(poss, cmpl_state);

  return poss;
}

static PossibleCompletion*
cmpl_next_completion (CompletionState* cmpl_state)
{
  PossibleCompletion* poss = NULL;

  cmpl_state->the_completion.text[0] = 0;

  if(cmpl_state->user_completion_index >= 0)
    poss = attempt_homedir_completion(cmpl_state->last_completion_text, cmpl_state);
  else
    poss = attempt_file_completion(cmpl_state);

  update_cmpl(poss, cmpl_state);

  return poss;
}

/**********************************************************************/
/*			 Directory Operations                         */
/**********************************************************************/

/* Open the directory where completion will begin from, if possible. */
static CompletionDir*
open_ref_dir(gchar* text_to_complete,
	     gchar** remaining_text,
	     CompletionState* cmpl_state)
{
  gchar* first_slash;
  CompletionDir *new_dir;

  first_slash = strchr(text_to_complete, '/');

  if (text_to_complete[0] == '~')
    {
      new_dir = open_user_dir(text_to_complete, cmpl_state);

      if(new_dir)
	{
	  if(first_slash)
	    *remaining_text = first_slash + 1;
	  else
	    *remaining_text = text_to_complete + strlen(text_to_complete);
	}
      else
	{
	  return NULL;
	}
    }
  else if (text_to_complete[0] == '/' || !cmpl_state->reference_dir)
    {
      gchar *tmp = g_strdup(text_to_complete);
      gchar *p;

      p = tmp;
      while (*p && *p != '*' && *p != '?')
	p++;

      *p = '\0';
      p = strrchr(tmp, '/');
      if (p)
	{
	  if (p == tmp)
	    p++;
      
	  *p = '\0';

	  new_dir = open_dir(tmp, cmpl_state);

	  if(new_dir)
	    *remaining_text = text_to_complete + 
	      ((p == tmp + 1) ? (p - tmp) : (p + 1 - tmp));
	}
      else
	{
	  /* If no possible candidates, use the cwd */
	  gchar *curdir = g_get_current_dir ();
	  
	  new_dir = open_dir(curdir, cmpl_state);

	  if (new_dir)
	    *remaining_text = text_to_complete;

	  g_free (curdir);
	}

      g_free (tmp);
    }
  else
    {
      *remaining_text = text_to_complete;

      new_dir = open_dir(cmpl_state->reference_dir->fullname, cmpl_state);
    }

  if(new_dir)
    {
      new_dir->cmpl_index = -1;
      new_dir->cmpl_parent = NULL;
    }

  return new_dir;
}

/* open a directory by user name */
static CompletionDir*
open_user_dir(gchar* text_to_complete,
	      CompletionState *cmpl_state)
{
  gchar *first_slash;
  gint cmp_len;

  g_assert(text_to_complete && text_to_complete[0] == '~');

  first_slash = strchr(text_to_complete, '/');

  if (first_slash)
    cmp_len = first_slash - text_to_complete - 1;
  else
    cmp_len = strlen(text_to_complete + 1);

  if(!cmp_len)
    {
      /* ~/ */
      gchar *homedir = g_get_home_dir ();

      if (homedir)
	return open_dir(homedir, cmpl_state);
      else
	return NULL;
    }
  else
    {
      /* ~user/ */
      char* copy = g_new(char, cmp_len + 1);
      struct passwd *pwd;
      strncpy(copy, text_to_complete + 1, cmp_len);
      copy[cmp_len] = 0;
      pwd = getpwnam(copy);
      g_free(copy);
      if (!pwd)
	{
	  cmpl_errno = errno;
	  return NULL;
	}

      return open_dir(pwd->pw_dir, cmpl_state);
    }
}

/* open a directory relative the the current relative directory */
static CompletionDir*
open_relative_dir(gchar* dir_name,
		  CompletionDir* dir,
		  CompletionState *cmpl_state)
{
  gchar path_buf[2*MAXPATHLEN];

  if(dir->fullname_len + strlen(dir_name) + 2 >= MAXPATHLEN)
    {
      cmpl_errno = CMPL_ERRNO_TOO_LONG;
      return NULL;
    }

  strcpy(path_buf, dir->fullname);

  if(dir->fullname_len > 1)
    {
      path_buf[dir->fullname_len] = '/';
      strcpy(path_buf + dir->fullname_len + 1, dir_name);
    }
  else
    {
      strcpy(path_buf + dir->fullname_len, dir_name);
    }

  return open_dir(path_buf, cmpl_state);
}

/* after the cache lookup fails, really open a new directory */
static CompletionDirSent*
open_new_dir(gchar* dir_name, struct stat* sbuf, gboolean stat_subdirs)
{
  CompletionDirSent* sent;
  DIR* directory;
  gchar *buffer_ptr;
  struct dirent *dirent_ptr;
  gint buffer_size = 0;
  gint entry_count = 0;
  gint i;
  struct stat ent_sbuf;
  char path_buf[MAXPATHLEN*2];
  gint path_buf_len;

  sent = g_new(CompletionDirSent, 1);
  sent->mtime = sbuf->st_mtime;
  sent->inode = sbuf->st_ino;
  sent->device = sbuf->st_dev;

  path_buf_len = strlen(dir_name);

  if (path_buf_len > MAXPATHLEN)
    {
      cmpl_errno = CMPL_ERRNO_TOO_LONG;
      return NULL;
    }

  strcpy(path_buf, dir_name);

  directory = opendir(dir_name);

  if(!directory)
    {
      cmpl_errno = errno;
      return NULL;
    }

  while((dirent_ptr = readdir(directory)) != NULL)
    {
      int entry_len = strlen(dirent_ptr->d_name);
      buffer_size += entry_len + 1;
      entry_count += 1;

      if(path_buf_len + entry_len + 2 >= MAXPATHLEN)
	{
	  cmpl_errno = CMPL_ERRNO_TOO_LONG;
 	  closedir(directory);
	  return NULL;
	}
    }

  sent->name_buffer = g_new(gchar, buffer_size);
  sent->entries = g_new(CompletionDirEntry, entry_count);
  sent->entry_count = entry_count;

  buffer_ptr = sent->name_buffer;

  rewinddir(directory);

  for(i = 0; i < entry_count; i += 1)
    {
      dirent_ptr = readdir(directory);

      if(!dirent_ptr)
	{
	  cmpl_errno = errno;
	  closedir(directory);
	  return NULL;
	}

      strcpy(buffer_ptr, dirent_ptr->d_name);
      sent->entries[i].entry_name = buffer_ptr;
      buffer_ptr += strlen(dirent_ptr->d_name);
      *buffer_ptr = 0;
      buffer_ptr += 1;

      path_buf[path_buf_len] = '/';
      strcpy(path_buf + path_buf_len + 1, dirent_ptr->d_name);

      if (stat_subdirs)
	{
	  if(stat(path_buf, &ent_sbuf) >= 0 && S_ISDIR(ent_sbuf.st_mode))
	    sent->entries[i].is_dir = 1;
	  else
	    /* stat may fail, and we don't mind, since it could be a
	     * dangling symlink. */
	    sent->entries[i].is_dir = 0;
	}
      else
	sent->entries[i].is_dir = 1;
    }

  qsort(sent->entries, sent->entry_count, sizeof(CompletionDirEntry), compare_cmpl_dir);

  closedir(directory);

  return sent;
}

static gboolean
check_dir(gchar *dir_name, struct stat *result, gboolean *stat_subdirs)
{
  /* A list of directories that we know only contain other directories.
   * Trying to stat every file in these directories would be very
   * expensive.
   */

  static struct {
    gchar *name;
    gboolean present;
    struct stat statbuf;
  } no_stat_dirs[] = {
    { "/afs", FALSE, { 0 } },
    { "/net", FALSE, { 0 } }
  };

  static const gint n_no_stat_dirs = sizeof(no_stat_dirs) / sizeof(no_stat_dirs[0]);
  static gboolean initialized = FALSE;

  gint i;

  if (!initialized)
    {
      initialized = TRUE;
      for (i = 0; i < n_no_stat_dirs; i++)
	{
	  if (stat (no_stat_dirs[i].name, &no_stat_dirs[i].statbuf) == 0)
	    no_stat_dirs[i].present = TRUE;
	}
    }

  if(stat(dir_name, result) < 0)
    {
      cmpl_errno = errno;
      return FALSE;
    }

  *stat_subdirs = TRUE;
  for (i=0; i<n_no_stat_dirs; i++)
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

/* open a directory by absolute pathname */
static CompletionDir*
open_dir(gchar* dir_name, CompletionState* cmpl_state)
{
  struct stat sbuf;
  gboolean stat_subdirs;
  CompletionDirSent *sent;
  GList* cdsl;

  if (!check_dir (dir_name, &sbuf, &stat_subdirs))
    return NULL;

  cdsl = cmpl_state->directory_sent_storage;

  while (cdsl)
    {
      sent = cdsl->data;

      if(sent->inode == sbuf.st_ino &&
	 sent->mtime == sbuf.st_mtime &&
	 sent->device == sbuf.st_dev)
	return attach_dir(sent, dir_name, cmpl_state);

      cdsl = cdsl->next;
    }

  sent = open_new_dir(dir_name, &sbuf, stat_subdirs);

  if (sent) {
    cmpl_state->directory_sent_storage =
      g_list_prepend(cmpl_state->directory_sent_storage, sent);

    return attach_dir(sent, dir_name, cmpl_state);
  }

  return NULL;
}

static CompletionDir*
attach_dir(CompletionDirSent* sent, gchar* dir_name, CompletionState *cmpl_state)
{
  CompletionDir* new_dir;

  new_dir = g_new(CompletionDir, 1);

  cmpl_state->directory_storage =
    g_list_prepend(cmpl_state->directory_storage, new_dir);

  new_dir->sent = sent;
  new_dir->fullname = g_strdup(dir_name);
  new_dir->fullname_len = strlen(dir_name);

  return new_dir;
}

static gint
correct_dir_fullname(CompletionDir* cmpl_dir)
{
  gint length = strlen(cmpl_dir->fullname);
  struct stat sbuf;

  if (strcmp(cmpl_dir->fullname + length - 2, "/.") == 0)
    {
      if (length == 2) 
	{
	  strcpy(cmpl_dir->fullname, "/");
	  cmpl_dir->fullname_len = 1;
	  return TRUE;
	} else {
	  cmpl_dir->fullname[length - 2] = 0;
	}
    }
  else if (strcmp(cmpl_dir->fullname + length - 3, "/./") == 0)
    cmpl_dir->fullname[length - 2] = 0;
  else if (strcmp(cmpl_dir->fullname + length - 3, "/..") == 0)
    {
      if(length == 3)
	{
	  strcpy(cmpl_dir->fullname, "/");
	  cmpl_dir->fullname_len = 1;
	  return TRUE;
	}

      if(stat(cmpl_dir->fullname, &sbuf) < 0)
	{
	  cmpl_errno = errno;
	  return FALSE;
	}

      cmpl_dir->fullname[length - 2] = 0;

      if(!correct_parent(cmpl_dir, &sbuf))
	return FALSE;
    }
  else if (strcmp(cmpl_dir->fullname + length - 4, "/../") == 0)
    {
      if(length == 4)
	{
	  strcpy(cmpl_dir->fullname, "/");
	  cmpl_dir->fullname_len = 1;
	  return TRUE;
	}

      if(stat(cmpl_dir->fullname, &sbuf) < 0)
	{
	  cmpl_errno = errno;
	  return FALSE;
	}

      cmpl_dir->fullname[length - 3] = 0;

      if(!correct_parent(cmpl_dir, &sbuf))
	return FALSE;
    }

  cmpl_dir->fullname_len = strlen(cmpl_dir->fullname);

  return TRUE;
}

static gint
correct_parent(CompletionDir* cmpl_dir, struct stat *sbuf)
{
  struct stat parbuf;
  gchar *last_slash;
  gchar *new_name;
  gchar c = 0;

  last_slash = strrchr(cmpl_dir->fullname, '/');

  g_assert(last_slash);

  if(last_slash != cmpl_dir->fullname)
    { /* last_slash[0] = 0; */ }
  else
    {
      c = last_slash[1];
      last_slash[1] = 0;
    }

  if (stat(cmpl_dir->fullname, &parbuf) < 0)
    {
      cmpl_errno = errno;
      return FALSE;
    }

  if (parbuf.st_ino == sbuf->st_ino && parbuf.st_dev == sbuf->st_dev)
    /* it wasn't a link */
    return TRUE;

  if(c)
    last_slash[1] = c;
  /* else
    last_slash[0] = '/'; */

  /* it was a link, have to figure it out the hard way */

  new_name = find_parent_dir_fullname(cmpl_dir->fullname);

  if (!new_name)
    return FALSE;

  g_free(cmpl_dir->fullname);

  cmpl_dir->fullname = new_name;

  return TRUE;
}

static gchar*
find_parent_dir_fullname(gchar* dirname)
{
  gchar buffer[MAXPATHLEN];
  gchar buffer2[MAXPATHLEN];

#if defined(sun) && !defined(__SVR4)
  if(!getwd(buffer))
#else
  if(!getcwd(buffer, MAXPATHLEN))
#endif    
    {
      cmpl_errno = errno;
      return NULL;
    }

  if(chdir(dirname) != 0 || chdir("..") != 0)
    {
      cmpl_errno = errno;
      return NULL;
    }

#if defined(sun) && !defined(__SVR4)
  if(!getwd(buffer2))
#else
  if(!getcwd(buffer2, MAXPATHLEN))
#endif
    {
      chdir(buffer);
      cmpl_errno = errno;

      return NULL;
    }

  if(chdir(buffer) != 0)
    {
      cmpl_errno = errno;
      return NULL;
    }

  return g_strdup(buffer2);
}

/**********************************************************************/
/*                        Completion Operations                       */
/**********************************************************************/

static PossibleCompletion*
attempt_homedir_completion(gchar* text_to_complete,
			   CompletionState *cmpl_state)
{
  gint index, length;

  if (!cmpl_state->user_dir_name_buffer &&
      !get_pwdb(cmpl_state))
    return NULL;
  length = strlen(text_to_complete) - 1;

  cmpl_state->user_completion_index += 1;

  while(cmpl_state->user_completion_index < cmpl_state->user_directories_len)
    {
      index = first_diff_index(text_to_complete + 1,
			       cmpl_state->user_directories
			       [cmpl_state->user_completion_index].login);

      switch(index)
	{
	case PATTERN_MATCH:
	  break;
	default:
	  if(cmpl_state->last_valid_char < (index + 1))
	    cmpl_state->last_valid_char = index + 1;
	  cmpl_state->user_completion_index += 1;
	  continue;
	}

      cmpl_state->the_completion.is_a_completion = 1;
      cmpl_state->the_completion.is_directory = 1;

      append_completion_text("~", cmpl_state);

      append_completion_text(cmpl_state->
			      user_directories[cmpl_state->user_completion_index].login,
			     cmpl_state);

      return append_completion_text("/", cmpl_state);
    }

  if(text_to_complete[1] ||
     cmpl_state->user_completion_index > cmpl_state->user_directories_len)
    {
      cmpl_state->user_completion_index = -1;
      return NULL;
    }
  else
    {
      cmpl_state->user_completion_index += 1;
      cmpl_state->the_completion.is_a_completion = 1;
      cmpl_state->the_completion.is_directory = 1;

      return append_completion_text("~/", cmpl_state);
    }
}

/* returns the index (>= 0) of the first differing character,
 * PATTERN_MATCH if the completion matches */
static gint
first_diff_index(gchar* pat, gchar* text)
{
  gint diff = 0;

  while(*pat && *text && *text == *pat)
    {
      pat += 1;
      text += 1;
      diff += 1;
    }

  if(*pat)
    return diff;

  return PATTERN_MATCH;
}

static PossibleCompletion*
append_completion_text(gchar* text, CompletionState* cmpl_state)
{
  gint len, i = 1;

  if(!cmpl_state->the_completion.text)
    return NULL;

  len = strlen(text) + strlen(cmpl_state->the_completion.text) + 1;

  if(cmpl_state->the_completion.text_alloc > len)
    {
      strcat(cmpl_state->the_completion.text, text);
      return &cmpl_state->the_completion;
    }

  while(i < len) { i <<= 1; }

  cmpl_state->the_completion.text_alloc = i;

  cmpl_state->the_completion.text = (gchar*)g_realloc(cmpl_state->the_completion.text, i);

  if(!cmpl_state->the_completion.text)
    return NULL;
  else
    {
      strcat(cmpl_state->the_completion.text, text);
      return &cmpl_state->the_completion;
    }
}

static CompletionDir*
find_completion_dir(gchar* text_to_complete,
		    gchar** remaining_text,
		    CompletionState* cmpl_state)
{
  gchar* first_slash = strchr(text_to_complete, '/');
  CompletionDir* dir = cmpl_state->reference_dir;
  CompletionDir* next;
  *remaining_text = text_to_complete;

  while(first_slash)
    {
      gint len = first_slash - *remaining_text;
      gint found = 0;
      gchar *found_name = NULL;         /* Quiet gcc */
      gint i;
      gchar* pat_buf = g_new (gchar, len + 1);

      strncpy(pat_buf, *remaining_text, len);
      pat_buf[len] = 0;

      for(i = 0; i < dir->sent->entry_count; i += 1)
	{
	  if(dir->sent->entries[i].is_dir &&
	     fnmatch(pat_buf, dir->sent->entries[i].entry_name,
		     FNMATCH_FLAGS)!= FNM_NOMATCH)
	    {
	      if(found)
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

      next = open_relative_dir(found_name, dir, cmpl_state);
      
      if(!next)
	{
	  g_free (pat_buf);
	  return NULL;
	}
      
      next->cmpl_parent = dir;
      
      dir = next;
      
      if(!correct_dir_fullname(dir))
	{
	  g_free(pat_buf);
	  return NULL;
	}
      
      *remaining_text = first_slash + 1;
      first_slash = strchr(*remaining_text, '/');

      g_free (pat_buf);
    }

  return dir;
}

static void
update_cmpl(PossibleCompletion* poss, CompletionState* cmpl_state)
{
  gint cmpl_len;

  if(!poss || !cmpl_is_a_completion(poss))
    return;

  cmpl_len = strlen(cmpl_this_completion(poss));

  if(cmpl_state->updated_text_alloc < cmpl_len + 1)
    {
      cmpl_state->updated_text =
	(gchar*)g_realloc(cmpl_state->updated_text,
			  cmpl_state->updated_text_alloc);
      cmpl_state->updated_text_alloc = 2*cmpl_len;
    }

  if(cmpl_state->updated_text_len < 0)
    {
      strcpy(cmpl_state->updated_text, cmpl_this_completion(poss));
      cmpl_state->updated_text_len = cmpl_len;
      cmpl_state->re_complete = cmpl_is_directory(poss);
    }
  else if(cmpl_state->updated_text_len == 0)
    {
      cmpl_state->re_complete = FALSE;
    }
  else
    {
      gint first_diff =
	first_diff_index(cmpl_state->updated_text,
			 cmpl_this_completion(poss));

      cmpl_state->re_complete = FALSE;

      if(first_diff == PATTERN_MATCH)
	return;

      if(first_diff > cmpl_state->updated_text_len)
	strcpy(cmpl_state->updated_text, cmpl_this_completion(poss));

      cmpl_state->updated_text_len = first_diff;
      cmpl_state->updated_text[first_diff] = 0;
    }
}

static PossibleCompletion*
attempt_file_completion(CompletionState *cmpl_state)
{
  gchar *pat_buf, *first_slash;
  CompletionDir *dir = cmpl_state->active_completion_dir;

  dir->cmpl_index += 1;

  if(dir->cmpl_index == dir->sent->entry_count)
    {
      if(dir->cmpl_parent == NULL)
	{
	  cmpl_state->active_completion_dir = NULL;

	  return NULL;
	}
      else
	{
	  cmpl_state->active_completion_dir = dir->cmpl_parent;

	  return attempt_file_completion(cmpl_state);
	}
    }

  g_assert(dir->cmpl_text);

  first_slash = strchr(dir->cmpl_text, '/');

  if(first_slash)
    {
      gint len = first_slash - dir->cmpl_text;

      pat_buf = g_new (gchar, len + 1);
      strncpy(pat_buf, dir->cmpl_text, len);
      pat_buf[len] = 0;
    }
  else
    {
      gint len = strlen(dir->cmpl_text);

      pat_buf = g_new (gchar, len + 2);
      strcpy(pat_buf, dir->cmpl_text);
      strcpy(pat_buf + len, "*");
    }

  if(first_slash)
    {
      if(dir->sent->entries[dir->cmpl_index].is_dir)
	{
	  if(fnmatch(pat_buf, dir->sent->entries[dir->cmpl_index].entry_name,
		     FNMATCH_FLAGS) != FNM_NOMATCH)
	    {
	      CompletionDir* new_dir;

	      new_dir = open_relative_dir(dir->sent->entries[dir->cmpl_index].entry_name,
					  dir, cmpl_state);

	      if(!new_dir)
		{
		  g_free (pat_buf);
		  return NULL;
		}

	      new_dir->cmpl_parent = dir;

	      new_dir->cmpl_index = -1;
	      new_dir->cmpl_text = first_slash + 1;

	      cmpl_state->active_completion_dir = new_dir;

	      g_free (pat_buf);
	      return attempt_file_completion(cmpl_state);
	    }
	  else
	    {
	      g_free (pat_buf);
	      return attempt_file_completion(cmpl_state);
	    }
	}
      else
	{
	  g_free (pat_buf);
	  return attempt_file_completion(cmpl_state);
	}
    }
  else
    {
      if(dir->cmpl_parent != NULL)
	{
	  append_completion_text(dir->fullname +
				 strlen(cmpl_state->completion_dir->fullname) + 1,
				 cmpl_state);
	  append_completion_text("/", cmpl_state);
	}

      append_completion_text(dir->sent->entries[dir->cmpl_index].entry_name, cmpl_state);

      cmpl_state->the_completion.is_a_completion =
	(fnmatch(pat_buf, dir->sent->entries[dir->cmpl_index].entry_name,
		 FNMATCH_FLAGS) != FNM_NOMATCH);

      cmpl_state->the_completion.is_directory = dir->sent->entries[dir->cmpl_index].is_dir;
      if(dir->sent->entries[dir->cmpl_index].is_dir)
	append_completion_text("/", cmpl_state);

      g_free (pat_buf);
      return &cmpl_state->the_completion;
    }
}


static gint
get_pwdb(CompletionState* cmpl_state)
{
  struct passwd *pwd_ptr;
  gchar* buf_ptr;
  gint len = 0, i, count = 0;

  if(cmpl_state->user_dir_name_buffer)
    return TRUE;
  setpwent ();

  while ((pwd_ptr = getpwent()) != NULL)
    {
      len += strlen(pwd_ptr->pw_name);
      len += strlen(pwd_ptr->pw_dir);
      len += 2;
      count += 1;
    }

  setpwent ();

  cmpl_state->user_dir_name_buffer = g_new(gchar, len);
  cmpl_state->user_directories = g_new(CompletionUserDir, count);
  cmpl_state->user_directories_len = count;

  buf_ptr = cmpl_state->user_dir_name_buffer;

  for(i = 0; i < count; i += 1)
    {
      pwd_ptr = getpwent();
      if(!pwd_ptr)
	{
	  cmpl_errno = errno;
	  goto error;
	}

      strcpy(buf_ptr, pwd_ptr->pw_name);
      cmpl_state->user_directories[i].login = buf_ptr;
      buf_ptr += strlen(buf_ptr);
      buf_ptr += 1;
      strcpy(buf_ptr, pwd_ptr->pw_dir);
      cmpl_state->user_directories[i].homedir = buf_ptr;
      buf_ptr += strlen(buf_ptr);
      buf_ptr += 1;
    }

  qsort(cmpl_state->user_directories,
	cmpl_state->user_directories_len,
	sizeof(CompletionUserDir),
	compare_user_dir);

  endpwent();

  return TRUE;

error:

  if(cmpl_state->user_dir_name_buffer)
    g_free(cmpl_state->user_dir_name_buffer);
  if(cmpl_state->user_directories)
    g_free(cmpl_state->user_directories);

  cmpl_state->user_dir_name_buffer = NULL;
  cmpl_state->user_directories = NULL;

  return FALSE;
}

static gint
compare_user_dir(const void* a, const void* b)
{
  return strcmp((((CompletionUserDir*)a))->login,
		(((CompletionUserDir*)b))->login);
}

static gint
compare_cmpl_dir(const void* a, const void* b)
{
  return strcmp((((CompletionDirEntry*)a))->entry_name,
		(((CompletionDirEntry*)b))->entry_name);
}

static gint
cmpl_state_okay(CompletionState* cmpl_state)
{
  return  cmpl_state && cmpl_state->reference_dir;
}

static gchar*
cmpl_strerror(gint err)
{
  if(err == CMPL_ERRNO_TOO_LONG)
    return "Name too long";
  else
    return g_strerror (err);
}
