/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativequartz.c: Quartz Native File selector dialog
 * Copyright (C) 2017, Tom Schoonjans
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include "gtkprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserwidgetprivate.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkstylecontext.h"
#include "gtkheaderbar.h"
#include "gtklabel.h"
#include "gtkfilechooserentry.h"
#include "gtkfilefilterprivate.h"
#include <quartz/gdkquartz.h>
#include <quartz/gdkquartz-cocoa-access.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
typedef struct {
  GtkFileChooserNative *self;

  NSSavePanel *panel;
  NSWindow *parent;
  NSWindow *key_window;
  gboolean skip_response;
  gboolean save;
  gboolean folder;
  gboolean create_folders;
  gboolean modal;
  gboolean overwrite_confirmation;
  gboolean select_multiple;
  gboolean show_hidden;
  gboolean running;

  char *accept_label;
  char *cancel_label;
  char *title;
  char *message;

  GFile *current_folder;
  GFile *current_file;
  char *current_name;

  NSMutableArray *filters;
  NSMutableArray *filter_names;
  NSComboBox *filter_combo_box;

  GSList *files;
  int response;
} FileChooserQuartzData;

@interface FilterComboBox : NSObject<NSComboBoxDelegate>
{
  FileChooserQuartzData *data;
}
- (id) initWithData:(FileChooserQuartzData *) quartz_data;
- (void)comboBoxSelectionDidChange:(NSNotification *)notification;
@end

@implementation FilterComboBox

- (id) initWithData:(FileChooserQuartzData *) quartz_data
{
  [super init];
  data = quartz_data;
  return self;
}
- (void)comboBoxSelectionDidChange:(NSNotification *)notification
{
  NSInteger selected_index = [data->filter_combo_box indexOfSelectedItem];
  NSArray *filter = [data->filters objectAtIndex:selected_index];
  // check for empty strings in filter -> indicates all filetypes should be allowed!
  if ([filter containsObject:@""])
    [data->panel setAllowedFileTypes:nil];
  else
    [data->panel setAllowedFileTypes:filter];

  GSList *filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (data->self));
  data->self->current_filter = g_slist_nth_data (filters, selected_index);
  g_slist_free (filters);
  g_object_notify (G_OBJECT (data->self), "filter");
}
@end

static GFile *
ns_url_to_g_file (NSURL *url)
{
  if (url == nil)
    {
      return NULL;
    }

  return g_file_new_for_uri ([[url absoluteString] UTF8String]);
}

static GSList *
chooser_get_files (FileChooserQuartzData *data)
{

  GSList *ret = NULL;

  if (!data->save)
    {
      NSArray *urls;
      gint i;

      urls = [(NSOpenPanel *)data->panel URLs];

      for (i = 0; i < [urls count]; i++)
        {
          NSURL *url;

          url = (NSURL *)[urls objectAtIndex:i];
          ret = g_slist_prepend (ret, ns_url_to_g_file (url));
        }
    }
  else
    {
      GFile *file;

      file = ns_url_to_g_file ([data->panel URL]);

      if (file != NULL)
        {
          ret = g_slist_prepend (ret, file);
        }
    }

  return g_slist_reverse (ret);
}

static void
chooser_set_current_folder (FileChooserQuartzData *data,
                            GFile                 *folder)
{

  if (folder != NULL)
    {
      gchar *uri;

      uri = g_file_get_uri (folder);
      [data->panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
      g_free (uri);
    }
}

static void
chooser_set_current_name (FileChooserQuartzData *data,
                          const gchar           *name)
{

  if (name != NULL)
    [data->panel setNameFieldStringValue:[NSString stringWithUTF8String:name]];
}


static void
filechooser_quartz_data_free (FileChooserQuartzData *data)
{
  
  if (data->filters)
    {
      [data->filters release];
    }

  if (data->filter_names)
    {
      [data->filter_names release];
    }
  
  g_clear_object (&data->current_folder);
  g_clear_object (&data->current_file);
  g_free (data->current_name);

  g_slist_free_full (data->files, g_object_unref);
  if (data->self)
    g_object_unref (data->self);
  g_free (data->accept_label);
  g_free (data->cancel_label);
  g_free (data->title);
  g_free (data->message);
  g_free (data);
}

@protocol CanSetAccessoryViewDisclosed
- (void)setAccessoryViewDisclosed:(BOOL)val;
@end

static gboolean
filechooser_quartz_launch (FileChooserQuartzData *data)
{

  if (data->save)
    {

      if (data->folder)
        {
          NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];
          [panel setCanChooseDirectories:YES];
          [panel setCanChooseFiles:NO];
          [panel setCanCreateDirectories:YES];
          data->panel = panel;
        }
      else
        {
          NSSavePanel *panel = [[NSSavePanel savePanel] retain];
          if (data->create_folders)
            {
              [panel setCanCreateDirectories:YES];
            }
          else
            {
              [panel setCanCreateDirectories:NO];
            }
          data->panel = panel;
        }
    }
  else
    {
      NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];

      if (data->select_multiple)
        {
          [panel setAllowsMultipleSelection:YES];
        }
      if (data->folder)
        {
          [panel setCanChooseDirectories:YES];
          [panel setCanChooseFiles:NO];
        }
      else
      {
        [panel setCanChooseDirectories:NO];
        [panel setCanChooseFiles:YES];
      }

      data->panel = panel;
  }

  [data->panel setReleasedWhenClosed:YES];

  if (data->show_hidden)
    {
      [data->panel setShowsHiddenFiles:YES];
    }

  if (data->accept_label)
    [data->panel setPrompt:[NSString stringWithUTF8String:data->accept_label]];

  if (data->title)
    [data->panel setTitle:[NSString stringWithUTF8String:data->title]];

  if (data->message)
    [data->panel setMessage:[NSString stringWithUTF8String:data->message]];

  if (data->current_file)
    {
      GFile *folder;
      gchar *name;

      folder = g_file_get_parent (data->current_file);
      name = g_file_get_basename (data->current_file);

      chooser_set_current_folder (data, folder);
      chooser_set_current_name (data, name);

      g_object_unref (folder);
      g_free (name);
    }

  if (data->current_folder)
    {
      chooser_set_current_folder (data, data->current_folder);
    }

  if (data->current_name)
    {
      chooser_set_current_name (data, data->current_name);
    }

  if (data->filters)
    {
      // when filters have been provided, a combobox needs to be added
      data->filter_combo_box = [[NSComboBox alloc] initWithFrame:NSMakeRect(0.0, 0.0, 200, 20)];
      [data->filter_combo_box addItemsWithObjectValues:data->filter_names];
      [data->filter_combo_box setEditable:NO];
      [data->filter_combo_box setDelegate:[[FilterComboBox alloc] initWithData:data]];

      if (data->self->current_filter)
        {
          GSList *filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (data->self));
	  gint current_filter_index = g_slist_index (filters, data->self->current_filter);
	  g_slist_free (filters);

	  if (current_filter_index >= 0)
            [data->filter_combo_box selectItemAtIndex:current_filter_index];
	  else
            [data->filter_combo_box selectItemAtIndex:0];
        }
      else
        {
          [data->filter_combo_box selectItemAtIndex:0];
        }
      [data->filter_combo_box setToolTip:[NSString stringWithUTF8String:_("Select which types of files are shown")]];
      [data->panel setAccessoryView:data->filter_combo_box];
      if ([data->panel isKindOfClass:[NSOpenPanel class]] && [data->panel respondsToSelector:@selector(setAccessoryViewDisclosed:)])
        {
          [(id<CanSetAccessoryViewDisclosed>) data->panel setAccessoryViewDisclosed:YES];
        }
    }
  data->response = GTK_RESPONSE_CANCEL;

  
  void (^handler)(NSInteger ret) = ^(NSInteger result) {

    if (result == NSFileHandlingPanelOKButton)
      {
        // get selected files and update data->files
        data->response = GTK_RESPONSE_ACCEPT;
	data->files = chooser_get_files (data);
      }

    GtkFileChooserNative *self = data->self;

    self->mode_data = NULL;

    if (data->parent)
      {
        [data->panel orderOut:nil];
        [data->parent makeKeyAndOrderFront:nil];
      }
    else
      {
        [data->key_window makeKeyAndOrderFront:nil];
      }

    if (!data->skip_response)
      {
        g_slist_free_full (self->custom_files, g_object_unref);
        self->custom_files = data->files;
        data->files = NULL;

        _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (data->self),
                                          data->response);
      }
    // free data!
    filechooser_quartz_data_free (data);
  };

  if (data->parent != NULL && data->modal)
    {
      [data->panel beginSheetModalForWindow:data->parent completionHandler:handler];
    }
  else
    {
      [data->panel beginWithCompletionHandler:handler];
    }

  return TRUE;
}

static gchar *
strip_mnemonic (const gchar *s)
{
  gchar *escaped;
  gchar *ret = NULL;

  if (s == NULL)
    return NULL;

  escaped = g_markup_escape_text (s, -1);
  pango_parse_markup (escaped, -1, '_', NULL, &ret, NULL, NULL);

  if (ret != NULL)
    {
      return ret;
    }
  else
    {
      return g_strdup (s);
    }
} 

static gboolean
file_filter_to_quartz (GtkFileFilter *file_filter,
                       NSMutableArray *filters,
		       NSMutableArray *filter_names)
{
  const char *name;
  NSArray *pattern_nsstrings;

  pattern_nsstrings = _gtk_file_filter_get_as_pattern_nsstrings (file_filter);
  if (pattern_nsstrings == NULL)
    return FALSE;

  name = gtk_file_filter_get_name (file_filter);
  NSString *name_nsstring;
  if (name == NULL)
    {
      name_nsstring = [pattern_nsstrings componentsJoinedByString:@","];;
    }
  else
    {
      name_nsstring = [NSString stringWithUTF8String:name];
      [name_nsstring retain];
    }

  [filter_names addObject:name_nsstring];
  [filters addObject:pattern_nsstrings];

  return TRUE;
}

gboolean
gtk_file_chooser_native_quartz_show (GtkFileChooserNative *self)
{
  FileChooserQuartzData *data;
  GtkWindow *transient_for;
  GtkFileChooserAction action;

  guint update_preview_signal;
  GSList *filters, *l;
  int n_filters, i;
  GtkWidget *extra_widget = NULL;
  char *message = NULL;

  /* Not supported before MacOS X 10.6 */
  if (gdk_quartz_osx_version () < GDK_OSX_SNOW_LEOPARD)
    return FALSE;

  extra_widget = gtk_file_chooser_get_extra_widget (GTK_FILE_CHOOSER (self));
  // if the extra_widget is a GtkLabel, then use its text to set the dialog message
  if (extra_widget != NULL)
    {
      if (!GTK_IS_LABEL (extra_widget))
        return FALSE;
      else
        message = g_strdup (gtk_label_get_text (GTK_LABEL (extra_widget)));
    }

  update_preview_signal = g_signal_lookup ("update-preview", GTK_TYPE_FILE_CHOOSER);
  if (g_signal_has_handler_pending (self, update_preview_signal, 0, TRUE))
    return FALSE;

  data = g_new0 (FileChooserQuartzData, 1);

  // examine filters!
  filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (self));
  n_filters = g_slist_length (filters);
  if (n_filters > 0)
    {
      data->filters = [NSMutableArray arrayWithCapacity:n_filters];
      [data->filters retain];
      data->filter_names = [NSMutableArray arrayWithCapacity:n_filters];
      [data->filter_names retain];

      for (l = filters, i = 0; l != NULL; l = l->next, i++)
        {
          if (!file_filter_to_quartz (l->data, data->filters, data->filter_names))
            {
              filechooser_quartz_data_free (data);
              return FALSE;
            }
        }
      self->current_filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (self));
    }
  else
    {
      self->current_filter = NULL;
    }
  self->mode_data = data;
  data->self = g_object_ref (self);

  data->create_folders = gtk_file_chooser_get_create_folders (GTK_FILE_CHOOSER (self));

  // shortcut_folder_uris support seems difficult if not impossible 

  // mnemonics are not supported on macOS, so remove the underscores
  data->accept_label = strip_mnemonic (self->accept_label);
  // cancel button is not present in macOS filechooser dialogs!
  // data->cancel_label = strip_mnemonic (self->cancel_label);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->dialog));

  if (action == GTK_FILE_CHOOSER_ACTION_SAVE ||
      action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    data->save = TRUE;

  if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    data->folder = TRUE;

  if ((action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
       action == GTK_FILE_CHOOSER_ACTION_OPEN) &&
      gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self->dialog)))
    data->select_multiple = TRUE;

  // overwrite confirmation appears to be always on
  if (gtk_file_chooser_get_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->dialog)))
    data->overwrite_confirmation = TRUE;

  if (gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (self->dialog)))
    data->show_hidden = TRUE;

  transient_for = gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self));
  if (transient_for)
    {
      gtk_widget_realize (GTK_WIDGET (transient_for));
      data->parent = gdk_quartz_window_get_nswindow (gtk_widget_get_window (GTK_WIDGET (transient_for)));

      if (gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)))
        data->modal = TRUE;
    }

  data->title =
    g_strdup (gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)));

  data->message = message;

  if (self->current_file)
    data->current_file = g_object_ref (self->current_file);
  else
    {
      if (self->current_folder)
        data->current_folder = g_object_ref (self->current_folder);

      if (action == GTK_FILE_CHOOSER_ACTION_SAVE ||
          action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
        data->current_name = g_strdup (self->current_name);
    }

  data->key_window = [NSApp keyWindow];

  return filechooser_quartz_launch(data);
}

void
gtk_file_chooser_native_quartz_hide (GtkFileChooserNative *self)
{
  FileChooserQuartzData *data = self->mode_data;

  /* Not supported before MacOS X 10.6 */
  if (gdk_quartz_osx_version () < GDK_OSX_SNOW_LEOPARD)
    return;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  data->skip_response = TRUE;
  if (data->panel == NULL)
    return;

  [data->panel orderBack:nil];
  [data->panel close];

  if (data->parent)
    {
      [data->parent makeKeyAndOrderFront:nil];
    }
  else
    {
      [data->key_window makeKeyAndOrderFront:nil];
    }
  data->panel = NULL;
}

#endif /* MAC_OS_X_VERSION_MAX_ALLOWED >= 1060 */
