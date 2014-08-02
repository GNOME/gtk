#include "iconbrowserapp.h"
#include "iconbrowserwin.h"
#include <gtk/gtk.h>

typedef struct
{
  const gchar *id;
  const gchar *name;
  const gchar *description;
} Context;

struct _IconBrowserWindow
{
  GtkApplicationWindow parent;
  GHashTable *contexts;

  GtkWidget *context_list;
  Context *current_context;
  gboolean symbolic;
  GtkWidget *symbolic_radio;
  GtkTreeModelFilter *filter_model;
  GtkWidget *details;

  GtkListStore *store;
  GtkCellRenderer *cell;
  GtkCellRenderer *text_cell;
  GtkWidget *search;
  GtkWidget *searchbar;
  GtkWidget *searchentry;
  GtkWidget *list;
  GtkWidget *image1;
  GtkWidget *image2;
  GtkWidget *image3;
  GtkWidget *image4;
  GtkWidget *image5;
  GtkWidget *description;
};

struct _IconBrowserWindowClass
{
  GtkApplicationWindowClass parent_class;
};

enum {
  NAME_COLUMN,
  SYMBOLIC_NAME_COLUMN,
  DESCRIPTION_COLUMN,
  CONTEXT_COLUMN
};

G_DEFINE_TYPE(IconBrowserWindow, icon_browser_window, GTK_TYPE_APPLICATION_WINDOW);

static void
search_text_changed (GtkEntry *entry)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);

  if (text[0] == '\0')
    return;
}

static void
set_image (GtkWidget *image, const gchar *name, gint size)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (image), name, 1);
  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
}

static void
selection_changed (GtkIconView *icon_view, IconBrowserWindow *win)
{
}

static void
item_activated (GtkIconView *icon_view, GtkTreePath *path, IconBrowserWindow *win)
{
  GtkTreeIter iter;
  gchar *name;
  gchar *description;
  gint column;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->filter_model), &iter, path);

  if (win->symbolic)
    column = SYMBOLIC_NAME_COLUMN;
  else
    column = NAME_COLUMN;
  gtk_tree_model_get (GTK_TREE_MODEL (win->filter_model), &iter,
                      column, &name,
                      DESCRIPTION_COLUMN, &description,
                      -1);
      
  if (name == NULL || !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name))
    {
      g_free (description);
      return;
    }

  gtk_window_set_title (GTK_WINDOW (win->details), name);
  set_image (win->image1, name, 16);
  set_image (win->image2, name, 24);
  set_image (win->image3, name, 32);
  set_image (win->image4, name, 48);
  set_image (win->image5, name, 64);
  if (description && description[0])
    {
      gtk_label_set_text (GTK_LABEL (win->description), description);
      gtk_widget_show (win->description);
    }
  else
    {
      gtk_widget_hide (win->description);
    }

  gtk_window_present (GTK_WINDOW (win->details));

  g_free (name);
  g_free (description);
}

static void
add_icon (IconBrowserWindow *win,
          const gchar       *name,
          const gchar       *description,
          const gchar       *context)
{
  gchar *regular_name;
  gchar *symbolic_name;

  regular_name = g_strdup (name);
  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), regular_name))
    {
      g_free (regular_name);
      regular_name = NULL;
    }

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), symbolic_name))
    {
      g_free (symbolic_name);
      symbolic_name = NULL;
    }
  gtk_list_store_insert_with_values (win->store, NULL, -1,
                                     NAME_COLUMN, regular_name,
                                     SYMBOLIC_NAME_COLUMN, symbolic_name,
                                     DESCRIPTION_COLUMN, description,
                                     CONTEXT_COLUMN, context,
                                     -1);
}

static void
add_context (IconBrowserWindow *win,
             const gchar       *id,
             const gchar       *name,
             const gchar       *description)
{
  Context *c;
  GtkWidget *row;

  c = g_new (Context, 1);
  c->id = id;
  c->name = name;
  c->description = description;

  g_hash_table_insert (win->contexts, (gpointer)id, c);

  row = gtk_label_new (name);
  g_object_set_data (G_OBJECT (row), "context", c);
  gtk_widget_show (row);
  g_object_set (row, "margin", 10, NULL);

  gtk_list_box_insert (GTK_LIST_BOX (win->context_list), row, -1);

  /* set the tooltip on the list box row */
  row = gtk_widget_get_parent (row);
  gtk_widget_set_tooltip_text (row, description);

  if (win->current_context == NULL)
    win->current_context = c;
}

static void
selected_context_changed (GtkListBox *list, IconBrowserWindow *win)
{
  GtkWidget *row;
  GtkWidget *label;

  row = GTK_WIDGET (gtk_list_box_get_selected_row (list));
  if (row == NULL)
    return;

  label = gtk_bin_get_child (GTK_BIN (row));
  win->current_context = g_object_get_data (G_OBJECT (label), "context");
  gtk_tree_model_filter_refilter (win->filter_model);
}

static void
populate (IconBrowserWindow *win)
{
  add_context (win, "volume", "Volume", "Icons related to audio input and output volume");
  add_icon (win, "audio-volume-high", "The icon used to indicate high audio volume", "volume");
  add_icon (win, "audio-volume-low", "The icon used to indicate low audio volume", "volume");
  add_icon (win, "audio-volume-medium", "The icon used to indicate medium audio volume", "volume");
  add_icon (win, "audio-volume-muted", "The icon used to indicate the muted state for audio playback", "volume");
  add_icon (win, "microphone-sensitivity-high", "The icon used to indicate high microphone sensitivity", "volume");
  add_icon (win, "microphone-sensitivity-low", "The icon used to indicate low microphone sensitivity", "volume");
  add_icon (win, "microphone-sensitivity-medium", "The icon used to indicate medium microphone sensitivity", "volume");
  add_icon (win, "microphone-sensitivity-muted", "The icon used to indicate that a microphone is muted", "volume");

  add_context (win, "multimedia", "Multimedia", "Icons related to playback of media");
  add_icon (win, "media-playlist-repeat", "The icon for the repeat mode of a media player", "multimedia");
  add_icon (win, "media-playlist-repeat-song", "The icon for repeating a song in a media player", "multimedia");
  add_icon (win, "media-playlist-shuffle", "The icon for the shuffle mode of a media player", "multimedia");
  add_icon (win, "media-playlist-consecutive", "The icon for consecutive mode of a media player", "multimedia");
  add_icon (win, "media-skip-backward", "The icon for the skip backward action of a media player", "multimedia");
  add_icon (win, "media-seek-backward", "The icon for the seek backward action of a media player", "multimedia");
  add_icon (win, "media-playback-start", "The icon for the start playback action of a media player", "multimedia");
  add_icon (win, "media-seek-forward", "The icon for the seek forward action of a media player", "multimedia");
  add_icon (win, "media-skip-forward", "The icon for the skip forward action of a media player", "multimedia");
  add_icon (win, "media-playback-stop", "The icon for the stop action of a media player", "multimedia");
  add_icon (win, "media-playback-pause", "The icon for the pause action of a media player", "multimedia");
  add_icon (win, "media-eject", "The icon for the eject action of a media player or file manager", "multimedia");
  add_icon (win, "media-record", "The icon for the record action of a media application", "multimedia");
  add_icon (win, "media-view-subtitles", "The icon used to show subtitles in a media player", "multimedia");

  add_context (win, "network", "Network", "Icons related to network status");
  add_icon (win, "network-transmit-receive", "The icon used data is being both transmitted and received simultaneously, while the computing device is connected to a network", "network");
  add_icon (win, "network-transmit", "The icon used when data is being transmitted, while the computing device is connected to a network", "network");
  add_icon (win, "network-receive", "The icon used when data is being received, while the computing device is connected to a network", "network");
  add_icon (win, "network-idle", "The icon used when no data is being transmitted or received, while the computing device is connected to a network", "network");
 add_icon (win, "network-error", "The icon used when an error occurs trying to intialize the network connection of the computing device", "network");
  add_icon (win, "network-offline", "The icon used when the computing device is disconnected from the network", "network");

  add_context (win, "weather", "Weather", "Icons about weather conditions");
  add_icon (win, "weather-clear", "The icon used while the weather for a region is “clear skies”", "weather");
  add_icon (win, "weather-clear-night", "The icon used while the weather for a region is “clear skies” during the night", "weather");
  add_icon (win, "weather-few-clouds", "The icon used while the weather for a region is “partly cloudy”", "weather");
  add_icon (win, "weather-few-clouds-night", "The icon used while the weather for a region is “partly cloudy” during the night", "weather");
  add_icon (win, "weather-fog", "The icon used while the weather for a region is “foggy”", "weather");
  add_icon (win, "weather-overcast", "The icon used while the weather for a region is “overcast”", "weather");
  add_icon (win, "weather-severe-alert", "The icon used while a sever weather alert is in effect for a region", "weather");
  add_icon (win, "weather-showers", "The icon used while rain showers are occurring in a region", "weather");
  add_icon (win, "weather-showers-scattered", "The icon used while scattered rain showers are occurring in a region", "weather");
  add_icon (win, "weather-snow", "The icon used while snow showers are occurring in a region", "weather");
  add_icon (win, "weather-storm", "The icon used while storms are occurring in a region", "weather");

  add_context (win, "navigation", "Navigation", "Icons for navigation in the user interface of a program");
  add_icon (win, "go-first", "The icon for the go to the first item in a list", "navigation");
  add_icon (win, "go-previous", "The icon for the go to the previous item in a list", "navigation");
  add_icon (win, "go-next", "The icon for the go to the next item in a list", "navigation");
  add_icon (win, "go-last", "The icon for the go to the last item in a list", "navigation");
  add_icon (win, "go-bottom", "The icon for the go to bottom of a list", "navigation");
  add_icon (win, "go-down", "The icon for the go down in a list", "navigation");
  add_icon (win, "go-up", "The icon for the go up in a list", "navigation");
  add_icon (win, "go-top", "The icon for the go to the top of a list", "navigation");
  add_icon (win, "go-home", "The icon for the go to home location", "navigation");
  add_icon (win, "go-jump", "The icon for the jump to action", "navigation");

  add_context (win, "editing", "Editing", "Icons related to editing a document");
  add_icon (win, "format-indent-less", "The icon for the decrease indent formatting action", "editing");
  add_icon (win, "format-indent-more", "The icon for the increase indent formatting action", "editing");
  add_icon (win, "format-justify-center", "The icon for the center justification formatting action", "editing");
  add_icon (win, "format-justify-fill", "The icon for the fill justification formatting action", "editing");
  add_icon (win, "format-justify-left", "The icon for the left justification formatting action", "editing");
  add_icon (win, "format-justify-right", "The icon for the right justification action", "editing");
  add_icon (win, "format-text-direction-ltr", "The icon for the left-to-right text formatting action", "editing");
  add_icon (win, "format-text-direction-rtl", "The icon for the right-to-left formatting action", "editing");
  add_icon (win, "format-text-bold", "The icon for the bold text formatting action", "editing");
  add_icon (win, "format-text-italic", "The icon for the italic text formatting action", "editing");
  add_icon (win, "format-text-underline", "The icon for the underlined text formatting action", "editing");
  add_icon (win, "format-text-strikethrough", "The icon for the strikethrough text formatting action", "editing");
  add_icon (win, "edit-clear", "The icon for the clear action", "editing");
  add_icon (win, "edit-clear-all", "", "editing");
  add_icon (win, "edit-copy", "The icon for the copy action", "editing");
  add_icon (win, "edit-cut", "The icon for the cut action", "editing");
  add_icon (win, "edit-delete", "The icon for the delete action", "editing");
  add_icon (win, "edit-find-replace", "The icon for the find and replace action", "editing");
  add_icon (win, "edit-paste", "The icon for the paste action", "editing");
  add_icon (win, "edit-redo", "The icon for the redo action", "editing");
  add_icon (win, "edit-select-all", "The icon for the select all action", "editing");
  add_icon (win, "edit-select", "", "editing");
  add_icon (win, "edit-undo", "The icon for the undo action", "editing");
  add_icon (win, "document-properties", "The icon for the action to view the properties of a document in an application", "editing");
  add_icon (win, "document-new", "The icon used for the action to create a new document", "editing");
  add_icon (win, "document-open", "The icon used for the action to open a document", "editing");
  add_icon (win, "document-open-recent", "The icon used for the action to open a document that was recently opened", "editing");
  add_icon (win, "document-save", "The icon for the save action. Should be an arrow pointing down and toward a hard disk", "editing");
  add_icon (win, "document-save-as", "The icon for the save as action", "editing");
  add_icon (win, "document-send", "The icon for the send action. Should be an arrow pointing up and away from a hard disk", "editing");
  add_icon (win, "document-page-setup", "The icon for the page setup action of a document editor", "editing");
  add_icon (win, "changes-allow", "", "other");
  add_icon (win, "changes-prevent", "", "other");
  add_icon (win, "object-flip-horizontal", "The icon for the action to flip an object horizontally", "editing");
  add_icon (win, "object-flip-vertical", "The icon for the action to flip an object vertically", "editing");
  add_icon (win, "object-rotate-left", "The icon for the rotate left action performed on an object", "editing");
  add_icon (win, "object-rotate-right", "The icon for the rotate rigt action performed on an object", "editing");
  add_icon (win, "insert-image", "The icon for the insert image action of an application", "editing");
  add_icon (win, "insert-link", "The icon for the insert link action of an application", "editing");
  add_icon (win, "insert-object", "The icon for the insert object action of an application", "editing");
  add_icon (win, "insert-text", "The icon for the insert text action of an application", "editing");
  add_icon (win, "accessories-text-editor", "The icon used for the desktop's text editing accessory program", "editing");

  add_context (win, "view", "View Controls", "Icons for view controls in a user interface");
  add_icon (win, "view-list", "The icon used for “List“ view mode", "view");
  add_icon (win, "view-grid", "The icon used for “Grid“ view mode (as opposed to “List“)", "view");
  add_icon (win, "view-fullscreen", "The icon used for the “Fullscreen” item in the application's “View” menu", "view");
  add_icon (win, "view-restore", "The icon used by an application for leaving the fullscreen view, and returning to a normal windowed view", "view");
  add_icon (win, "zoom-fit-best", "The icon used for the “Best Fit” item in the application's “View” menu", "view");
  add_icon (win, "zoom-in", "The icon used for the “Zoom in” item in the application's “View” menu", "view");
  add_icon (win, "zoom-out", "The icon used for the “Zoom Out” item in the application's “View” menu ", "view");
  add_icon (win, "zoom-original", "The icon used for the “Original Size” item in the application's “View” menu", "view");
  add_icon (win, "view-continuous", "The icon used for a continuous view mode", "view");
  add_icon (win, "view-paged", "The icon used for a paged view mode (as opposed to continuous)", "view");
  add_icon (win, "view-dual", "The icon used for a side-by-side view of paginated content", "view");
  add_icon (win, "view-wrapped", "The icon used to indicate a wrap-around to the beginning", "view");

  add_context (win, "calendar", "Calendar, Tasks and Alarms", "Icons related to calendars, tasks and alarms");
  add_icon (win, "task-due", "The icon used when a task is due soon", "calendar");
  add_icon (win, "task-past-due", "The icon used when a task that was due, has been left incomplete", "calendar");
  add_icon (win, "appointment-soon", "The icon used when an appointment will occur soon", "calendar");
  add_icon (win, "appointment-missed", "The icon used when an appointment was missed", "calendar");
  add_icon (win, "alarm", "The icon used for alarms when a task or appointment is due", "calendar");

  add_context (win, "communication", "Communication", "Icons related email, phone calls, IM and other forms of communication");
  add_icon (win, "mail-unread", "The icon used for an electronic mail that is unread", "communication");
  add_icon (win, "mail-read", "The icon used for an electronic mail that is read", "communication");
  add_icon (win, "mail-replied", "The icon used for an electronic mail that has been replied to", "communication");
  add_icon (win, "mail-attachment", "The icon used for an electronic mail that contains attachments", "communication");
  add_icon (win, "mail-mark-important", "The icon for the mark as important action of an electronic mail application", "communication");
  add_icon (win, "mail-send", "The icon for the send action of an electronic mail application", "communication");
  add_icon (win, "mail-send-receive", "The icon for the send and receive action of an electronic mail application", "communication");
  add_icon (win, "call-start", "The icon used for initiating or accepting a call", "communication");
  add_icon (win, "call-stop", "The icon used for stopping a current call", "communication");
  add_icon (win, "call-missed", "The icon used to show a missed call", "communication");
  add_icon (win, "user-available", "The icon used when a user on a chat network is available to initiate a conversation with", "communication");
  add_icon (win, "user-offline", "The icon used when a user on a chat network is not available", "communication");
  add_icon (win, "user-idle", "The icon used when a user on a chat network has not been an active participant in any chats on the network, for an extended period of time", "communication");
  add_icon (win, "user-invisible", "The icon used when a user is on a chat network, but is invisible to others", "communication");
  add_icon (win, "user-busy", "The icon used when a user is on a chat network, and has marked himself as busy", "communication");
  add_icon (win, "user-away", "The icon used when a user on a chat network is away from their keyboard and the chat program", "communication");
  add_icon (win, "user-status-pending", "The icon used when the current user status on a chat network is not known", "communication");

  add_context (win, "devices", "Devices and Media", "Icons for devices and media");
  add_icon (win, "audio-input-microphone", "The icon used for the microphone audio input device", "devices");
  add_icon (win, "camera-web", "The fallback icon for web cameras", "devices");
  add_icon (win, "camera-photo", "The icon used for a digital still camera devices", "devices");
  add_icon (win, "input-keyboard", "The icon used for the keyboard input device", "devices");
  add_icon (win, "printer", "The icon used for a printer device", "devices");
  add_icon (win, "video-display", "The icon used for the monitor that video gets displayed to", "devices");
  add_icon (win, "computer", "The icon used for the computing device as a whole", "devices");
  add_icon (win, "media-optical", "The icon used for physical optical media such as CD and DVD", "devices");
  add_icon (win, "phone", "The icon used for phone devices which support connectivity to the PC, such as VoIP, cellular, or possibly landline phones", "devices");
  add_icon (win, "input-dialpad", "The icon used for dialpad input devices", "devices");
  add_icon (win, "input-touchpad", "The icon used for touchpad input devices", "devices");
  add_icon (win, "scanner", "The icon used for a scanner device", "devices");
  add_icon (win, "audio-card", "The icon used for the audio rendering device", "devices");
  add_icon (win, "input-gaming", "The icon used for the gaming input device", "devices");
  add_icon (win, "input-mouse", "The icon used for the mousing input device", "devices");
  add_icon (win, "multimedia-player", "The icon used for generic multimedia playing devices", "devices");
  add_icon (win, "audio-headphones", "The icon used for headphones", "devices");
  add_icon (win, "audio-headset", "The icon used for headsets", "devices");
  add_icon (win, "display-projector", "The icon used for projectors", "devices");
  add_icon (win, "media-removable", "The icon used for generic removable media", "devices");
  add_icon (win, "printer-network", "The icon used for printers which are connected via the network", "devices");
  add_icon (win, "audio-speakers", "The icon used for speakers", "devices");
  add_icon (win, "camera-video", "The fallback icon for video cameras", "devices");
  add_icon (win, "drive-optical", "The icon used for optical media drives such as CD and DVD", "devices");
  add_icon (win, "drive-removable-media", "The icon used for removable media drives", "devices");
  add_icon (win, "input-tablet", "The icon used for graphics tablet input devices", "devices");
  add_icon (win, "network-wireless", "The icon used for wireless network connections", "devices");
  add_icon (win, "network-wired", "The icon used for wired network connections", "devices");
  add_icon (win, "media-floppy", "The icon used for physical floppy disk media", "devices");
  add_icon (win, "media-flash", "The fallback icon used for flash media, such as memory stick and SD", "devices");

  add_context (win, "contenttypes", "Content Types", "Icons for different types of data, such as audio or image files");
  add_icon (win, "application-certificate", "", "contenttypes"); 
  add_icon (win, "application-rss+xml", "", "contenttypes"); 
  add_icon (win, "application-x-appliance", "", "contenttypes"); 
  add_icon (win, "audio-x-generic", "The icon used for generic audio file types", "contenttypes");
  add_icon (win, "folder", "The standard folder icon used to represent directories on local filesystems, mail folders, and other hierarchical groups", "contenttypes");
  add_icon (win, "text-x-generic", "The icon used for generic text file types", "contenttypes");
  add_icon (win, "video-x-generic", "The icon used for generic video file types", "contenttypes");
  add_icon (win, "x-office-calendar", "The icon used for generic calendar file types", "contenttypes");

  add_context (win, "emotes", "Emotes", "Icons for emotions that are expressed through text chat applications such as :-) or :-P in IRC or instant messengers");
  add_icon (win, "face-angel", "The icon used for the 0:-) emote", "emotes");
  add_icon (win, "face-angry", "The icon used for the X-( emote", "emotes");
  add_icon (win, "face-cool", "The icon used for the B-) emote", "emotes");
  add_icon (win, "face-crying", "The icon used for the :'( emote", "emotes");
  add_icon (win, "face-devilish", "The icon used for the >:-) emote", "emotes");
  add_icon (win, "face-embarrassed", "The icon used for the :-[ emote", "emotes");
  add_icon (win, "face-kiss", "The icon used for the :-* emote", "emotes");
  add_icon (win, "face-laugh", "The icon used for the :-)) emote", "emotes");
  add_icon (win, "face-monkey", "The icon used for the :-(|) emote", "emotes");
  add_icon (win, "face-plain", "The icon used for the :-| emote", "emotes");
  add_icon (win, "face-raspberry", "The icon used for the :-P emote", "emotes");
  add_icon (win, "face-sad", "The icon used for the :-( emote", "emotes");
  add_icon (win, "face-shutmouth", "The 'shut mouth' emote", "emotes"); 
  add_icon (win, "face-sick", "The icon used for the :-& emote", "emotes");
  add_icon (win, "face-smile", "The icon used for the :-) emote", "emotes");
  add_icon (win, "face-smile-big", "The icon used for the :-D emote", "emotes");
  add_icon (win, "face-smirk", "The icon used for the :-! emote", "emotes");
  add_icon (win, "face-surprise", "The icon used for the :-0 emote", "emotes");
  add_icon (win, "face-tired", "The icon used for the |-) emote", "emotes");
  add_icon (win, "face-uncertain", "The icon used for the :-/ emote", "emotes");
  add_icon (win, "face-wink", "The icon used for the ;-) emote", "emotes");
  add_icon (win, "face-worried", "The icon used for the :-S emote", "emotes");
  add_icon (win, "face-yawn", "", "emotes"); 

  add_context (win, "general", "General", "Generally useful icons that don't fit in a particular category");
  add_icon (win, "edit-find", "The icon for generic search actions", "general");
  add_icon (win, "content-loading", "The icon used to indicate that content is loading", "general"); 
  add_icon (win, "open-menu", "The icon used for a menu button in the header bar", "general"); 
  add_icon (win, "view-more", "The icon used for a  “View More“ action", "general"); 
  add_icon (win, "tab-new", "The icon used for a “New Tab“ action", "general"); 
  add_icon (win, "bookmark-new", "The icon used for creating a new bookmark", "general"); 
  add_icon (win, "mark-location", "The icon used to mark a location on a map", "general"); 
  add_icon (win, "find-location", "The icon used for a  “Search location“ action", "general"); 
  add_icon (win, "send-to", "The icon used for a  “Send to“ action", "general"); 
  add_icon (win, "object-select", "The icon used for generic selection actions", "general"); 
  add_icon (win, "window-close", "The icon used for actions that close a view, such as window or tab close button", "general");
  add_icon (win, "view-refresh", "The icon used for the “Refresh” item in the application's “View” menu", "general");
  add_icon (win, "process-stop", "The icon used for the “Stop” action in applications with actions that may take a while to process, such as web page loading in a browser", "general");
  add_icon (win, "action-unavailable", "The icon used to indicate that an action is currently unavailable, such as “Pause“ when no media is playing", "general"); 
  add_icon (win, "document-print", "The icon for the print action of an application", "general");
  add_icon (win, "printer-printing", "The icon used while a print job is successfully being spooled to a printing device", "general");
  add_icon (win, "printer-warning", "The icon used when a recoverable problem occurs while attempting to printing", "general"); 
  add_icon (win, "printer-error", "The icon used when an error occurs while attempting to print", "general");
  add_icon (win, "dialog-information", "The icon used when a dialog is opened to give information to the user that may be pertinent to the requested action", "general");
  add_icon (win, "dialog-question", "The icon used when a dialog is opened to ask a simple question of the user", "general");
  add_icon (win, "dialog-warning", "The icon used when a dialog is opened to warn the user of impending issues with the requested action", "general");
  add_icon (win, "dialog-password", "The icon used when a dialog requesting the authentication credentials for a user is opened", "general");
  add_icon (win, "dialog-error", "The icon used when a dialog is opened to explain an error condition to the user", "general");
  add_icon (win, "list-add", "The icon for the add to list action", "general");
  add_icon (win, "list-remove", "The icon for the remove from list action", "general");
  add_icon (win, "non-starred", "The icon used to indicate that an object is not 'starred'", "general"); 
  add_icon (win, "semi-starred", "The icon used to indicate that an object has is 'half-starred'", "general"); 
  add_icon (win, "starred", "The icon used to indicate that an object is 'starred'", "general"); 
  add_icon (win, "star-new", "The used for the “New Star“ action", "general"); 
  add_icon (win, "security-low", "The icon used to indicate that the security level of a connection is presumed to be insecure, either by using weak encryption, or by using a certificate that the could not be automatically verified, and which the user has not chosent to trust", "general");
  add_icon (win, "security-medium", "The icon used to indicate that the security level of a connection is presumed to be secure, using strong encryption, and a certificate that could not be automatically verified, but which the user has chosen to trust", "general");
  add_icon (win, "security-high", "The icon used to indicate that the security level of a connection is known to be secure, using strong encryption and a valid certificate", "general");
  add_icon (win, "user-trash", "The icon for the user's “Trash” place in the file system", "other");
  add_icon (win, "user-trash-full", "The icon for the user's “Trash” in the file system, when there are items in the “Trash” waiting for disposal or recovery", "general");
  add_icon (win, "emblem-system", "The icon used as an emblem for directories that contain system libraries, settings, and data", "general");
  add_icon (win, "avatar-default", "The generic avatar icon, which is used to represent a user that doesn't have a personalized avatar", "general"); 
  add_icon (win, "emblem-synchronizing", "The icon used as an emblem to indicate that a a synchronizing operation is in process", "general"); 
  add_icon (win, "emblem-shared", "The icon used as an emblem for files and directories that are shared to other users", "general");
  add_icon (win, "folder-download", "The icon representing the location in the file system where downloaded files are stored", "general"); 
  add_icon (win, "help-browser", "The icon used for the desktop's help browsing application", "general");

  add_context (win, "other", "Other", "Icons which have may be too specialized and not of general interest");
  add_icon (win, "view-sort-ascending", "The icon used for the “Sort Ascending” item in the application's “View” menu, or in a button for changing the sort method for a list", "other");
  add_icon (win, "view-sort-descending", "The icon used for the “Sort Descending” item in the application's “View” menu, or in a button for changing the sort method for a list", "other");
  add_icon (win, "document-revert", "The icon for the action of reverting to a previous version of a document", "other");
  add_icon (win, "address-book-new", "The icon used for the action to create a new address book", "other");
  add_icon (win, "application-exit", "The icon used for exiting an application. Typically this is seen in the application's menus as File->Quit", "other");
  add_icon (win, "appointment-new", "The icon used for the action to create a new appointment in a calendaring application", "other");
  add_icon (win, "contact-new", "The icon used for the action to create a new contact in an address book application", "other");
  add_icon (win, "document-print-preview", "The icon for the print preview action of an application", "other");
  add_icon (win, "folder-new", "The icon for creating a new folder", "other");
  add_icon (win, "help-about", "The icon for the About item in the Help menu", "other");
  add_icon (win, "help-contents", "The icon for Contents item in the Help menu", "other");
  add_icon (win, "help-faq", "The icon for the FAQ item in the Help menu", "other");
  add_icon (win, "list-remove-all", "", "other");
  add_icon (win, "mail-forward", "The icon for the forward action of an electronic mail application", "other");
  add_icon (win, "mail-mark-junk", "The icon for the mark as junk action of an electronic mail application", "other");
  add_icon (win, "mail-mark-notjunk", "The icon for the mark as not junk action of an electronic mail application", "other");
  add_icon (win, "mail-mark-read", "The icon for the mark as read action of an electronic mail application", "other");
  add_icon (win, "mail-mark-unread", "The icon for the mark as unread action of an electronic mail application", "other");
  add_icon (win, "mail-message-new", "The icon for the compose new mail action of an electronic mail application", "other");
  add_icon (win, "mail-reply-all", "The icon for the reply to all action of an electronic mail application", "other");
  add_icon (win, "mail-reply-sender", "The icon for the reply to sender action of an electronic mail application", "other");
  add_icon (win, "pan-down", "", "other"); 
  add_icon (win, "pan-end", "", "other"); 
  add_icon (win, "pan-start", "", "other"); 
  add_icon (win, "pan-up", "", "other"); 
  add_icon (win, "system-lock-screen", "The icon used for the “Lock Screen” item in the desktop's panel application", "other");
  add_icon (win, "system-log-out", "The icon used for the “Log Out” item in the desktop's panel application", "other");
  add_icon (win, "system-run", "The icon used for the “Run Application...” item in the desktop's panel application", "other");
  add_icon (win, "system-search", "The icon used for the “Search” item in the desktop's panel application", "other");
  add_icon (win, "system-reboot", "The icon used for the “Reboot” item in the desktop's panel application", "other");
  add_icon (win, "system-shutdown", "The icon used for the “Shutdown” item in the desktop's panel application", "other");
  add_icon (win, "tools-check-spelling", "The icon used for the “Check Spelling” item in the application's “Tools” menu", "other");
  add_icon (win, "window-maximize", "", "other"); 
  add_icon (win, "window-minimize", "", "other"); 
  add_icon (win, "window-restore", "", "other"); 
  add_icon (win, "window-new", "The icon used for the “New Window” item in the application's “Windows” menu", "other");
  add_icon (win, "accessories-calculator", "The icon used for the desktop's calculator accessory program", "other"); 
  add_icon (win, "accessories-character-map", "The icon used for the desktop's international and extended text character accessory program", "other");
  add_icon (win, "accessories-dictionary", "The icon used for the desktop's dictionary accessory program", "other");
  add_icon (win, "multimedia-volume-control", "The icon used for the desktop's hardware volume control application", "other");
  add_icon (win, "preferences-desktop-accessibility", "The icon used for the desktop's accessibility preferences", "other");
  add_icon (win, "preferences-desktop-display", "", "other"); 
  add_icon (win, "preferences-desktop-font", "The icon used for the desktop's font preferences", "other");
  add_icon (win, "preferences-desktop-keyboard", "The icon used for the desktop's keyboard preferences", "other");
  add_icon (win, "preferences-desktop-keyboard-shortcuts", "", "other"); 
  add_icon (win, "preferences-desktop-locale", "The icon used for the desktop's locale preferences", "other");
  add_icon (win, "preferences-desktop-remote-desktop", "", "other"); 
  add_icon (win, "preferences-desktop-multimedia", "The icon used for the desktop's multimedia preferences", "other");
  add_icon (win, "preferences-desktop-screensaver", "The icon used for the desktop's screen saving preferences", "other");
  add_icon (win, "preferences-desktop-theme", "The icon used for the desktop's theme preferences", "other");
  add_icon (win, "preferences-desktop-wallpaper", "The icon used for the desktop's wallpaper preferences", "other");
  add_icon (win, "preferences-system-privacy", "", "other"); 
  add_icon (win, "preferences-system-windows", "", "other"); 
  add_icon (win, "system-file-manager", "The icon used for the desktop's file management application", "other");
  add_icon (win, "system-software-install", "The icon used for the desktop's software installer application", "other");
  add_icon (win, "system-software-update", "The icon used for the desktop's software updating application", "other");
  add_icon (win, "system-users", "", "other"); 
  add_icon (win, "user-info", "", "other"); 
  add_icon (win, "utilities-system-monitor", "The icon used for the desktop's system resource monitor application", "other");
  add_icon (win, "utilities-terminal", "The icon used for the desktop's terminal emulation application. ", "other");
  add_icon (win, "application-x-addon", "", "other");
  add_icon (win, "application-x-executable", "The icon used for executable file types", "other");
  add_icon (win, "font-x-generic", "The icon used for generic font file types", "other");
  add_icon (win, "image-x-generic", "The icon used for generic image file types", "other");
  add_icon (win, "package-x-generic", "The icon used for generic package file types", "other");
  add_icon (win, "text-html", "The icon used for HTML text file types", "other");
  add_icon (win, "text-x-generic-template", "The icon used for generic text templates", "other");
  add_icon (win, "text-x-preview", "", "other"); 
  add_icon (win, "text-x-script", "The icon used for script file types, such as shell scripts", "other");
  add_icon (win, "x-office-address-book", "The icon used for generic address book file types", "other");
  add_icon (win, "x-office-document", "The icon used for generic document and letter file types", "other");
  add_icon (win, "x-office-document-template", "", "other"); 
  add_icon (win, "x-office-presentation", "The icon used for generic presentation file types", "other");
  add_icon (win, "x-office-presentation-template", "", "other"); 
  add_icon (win, "x-office-spreadsheet", "The icon used for generic spreadsheet file types", "other");
  add_icon (win, "x-office-spreadsheet-template", "", "other"); 
  add_icon (win, "x-package-repository", "", "other"); 
  add_icon (win, "applications-accessories", "The icon for the “Accessories” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-development", "The icon for the “Programming” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-engineering", "The icon for the “Engineering” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-games", "The icon for the “Games” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-graphics", "The icon for the “Graphics” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-internet", "The icon for the “Internet” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-multimedia", "The icon for the “Multimedia” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-office", "The icon for the “Office” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-other", "The icon for the “Other” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-science", "The icon for the “Science” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-system", "The icon for the “System Tools” sub-menu of the Programs menu", "other");
  add_icon (win, "applications-utilities", "The icon for the “Utilities” sub-menu of the Programs menu", "other");
  add_icon (win, "preferences-desktop", "The icon for the “Desktop Preferences” category", "other");
  add_icon (win, "preferences-desktop-peripherals", "The icon for the “Peripherals” sub-category of the “Desktop Preferences” category", "other");
  add_icon (win, "preferences-desktop-personal", "The icon for the “Personal” sub-category of the “Desktop Preferences” category", "other");
  add_icon (win, "preferences-other", "The icon for the “Other” preferences category", "other");
  add_icon (win, "preferences-system", "The icon for the “System Preferences” category", "other");
  add_icon (win, "preferences-system-network", "The icon for the “Network” sub-category of the “System Preferences” category", "other");
  add_icon (win, "system-help", "The icon for the “Help” system category", "other");
  add_icon (win, "battery", "The icon used for the system battery device", "other");
  add_icon (win, "computer-apple-ipad", "", "other"); 
  add_icon (win, "colorimeter-colorhug", "", "other"); 
  add_icon (win, "drive-harddisk", "The icon used for hard disk drives", "other");
  add_icon (win, "drive-harddisk-ieee1394", "", "other"); 
  add_icon (win, "drive-harddisk-system", "", "other"); 
  add_icon (win, "drive-multidisk", "", "other"); 
  add_icon (win, "media-optical-bd", "", "other"); 
  add_icon (win, "media-optical-cd-audio", "", "other"); 
  add_icon (win, "media-optical-dvd", "", "other"); 
  add_icon (win, "media-tape", "The icon used for generic physical tape media", "other");
  add_icon (win, "media-zip", "", "other"); 
  add_icon (win, "modem", "The icon used for modem devices", "other");
  add_icon (win, "multimedia-player-apple-ipod-touch", "", "other"); 
  add_icon (win, "network-vpn", "", "other"); 
  add_icon (win, "pda", "This is the fallback icon for Personal Digial Assistant devices. Primary use of this icon is for PDA devices connected to the PC. Connection medium is not an important aspect of the icon. The metaphor for this fallback icon should be a generic PDA device icon", "other");
  add_icon (win, "phone-apple-iphone", "", "other"); 
  add_icon (win, "uninterruptible-power-supply", "", "other"); 
  add_icon (win, "emblem-default", "The icon used as an emblem to specify the default selection of a printer for example", "other");
  add_icon (win, "emblem-documents", "The icon used as an emblem for the directory where a user's documents are stored", "other");
  add_icon (win, "emblem-downloads", "The icon used as an emblem for the directory where a user's downloads from the internet are stored", "other");
  add_icon (win, "emblem-favorite", "The icon used as an emblem for files and directories that the user marks as favorites", "other");
  add_icon (win, "emblem-generic", "", "other"); 
  add_icon (win, "emblem-important", "The icon used as an emblem for files and directories that are marked as important by the user", "other");
  add_icon (win, "emblem-mail", "The icon used as an emblem to specify the directory where the user's electronic mail is stored", "other");
  add_icon (win, "emblem-new", "", "other"); 
  add_icon (win, "emblem-ok", "", "other"); 
  add_icon (win, "emblem-package", "", "other"); 
  add_icon (win, "emblem-photos", "The icon used as an emblem to specify the directory where the user stores photographs", "other");
  add_icon (win, "emblem-readonly", "The icon used as an emblem for files and directories which can not be written to by the user", "other");
  add_icon (win, "emblem-symbolic-link", "The icon used as an emblem for files and direcotires that are links to other files or directories on the filesystem", "other");
  add_icon (win, "emblem-synchronized", "The icon used as an emblem for files or directories that are configured to be synchronized to another device", "other");
  add_icon (win, "emblem-unreadable", "The icon used as an emblem for files and directories that are inaccessible. ", "other");
  add_icon (win, "emblem-urgent", "", "other"); 
  add_icon (win, "emblem-videos", "", "other"); 
  add_icon (win, "emblem-web", "", "other"); 
  add_icon (win, "folder-documents", "", "other"); 
  add_icon (win, "folder-download", "", "other"); 
  add_icon (win, "folder-music", "", "other"); 
  add_icon (win, "folder-pictures", "", "other"); 
  add_icon (win, "folder-documents", "", "other"); 
  add_icon (win, "folder-publicshare", "", "other"); 
  add_icon (win, "folder-remote", "The icon used for normal directories on a remote filesystem", "other");
  add_icon (win, "folder-saved-search", "", "other"); 
  add_icon (win, "folder-templates", "", "other"); 
  add_icon (win, "folder-videos", "", "other"); 
  add_icon (win, "network-server", "The icon used for individual host machines under the “Network Servers” place in the file manager", "other");
  add_icon (win, "network-workgroup", "The icon for the “Network Servers” place in the desktop's file manager, and workgroups within the network", "other");
  add_icon (win, "start-here", "The icon used by the desktop's main menu for accessing places, applications, and other features", "other");
  add_icon (win, "user-bookmarks", "The icon for the user's special “Bookmarks” place", "other");
  add_icon (win, "user-desktop", "The icon for the special “Desktop” directory of the user", "other");
  add_icon (win, "user-home", "The icon for the special “Home” directory of the user", "other");
  add_icon (win, "airplane-mode", "", "other"); 
  add_icon (win, "battery-caution-charging", "", "other"); 
  add_icon (win, "battery-caution", "The icon used when the battery is below 40%", "other");
  add_icon (win, "battery-empty-charging", "", "other"); 
  add_icon (win, "battery-empty", "", "other"); 
  add_icon (win, "battery-full-charged", "", "other"); 
  add_icon (win, "battery-full-charging", "", "other"); 
  add_icon (win, "battery-full", "", "other"); 
  add_icon (win, "battery-good-charging", "", "other"); 
  add_icon (win, "battery-good", "", "other"); 
  add_icon (win, "battery-low-charging", "", "other"); 
  add_icon (win, "battery-low", "The icon used when the battery is below 20%", "other");
  add_icon (win, "battery-missing", "", "other"); 
  add_icon (win, "bluetooth-active", "", "other"); 
  add_icon (win, "bluetooth-disabled", "", "other"); 
  add_icon (win, "channel-insecure", "", "other"); 
  add_icon (win, "channel-secure", "", "other"); 
  add_icon (win, "computer-fail", "", "other"); 
  add_icon (win, "display-brightness", "", "other"); 
  add_icon (win, "keyboard-brightness", "", "other"); 
  add_icon (win, "folder-drag-accept", "The icon used for a folder while an object is being dragged onto it, that is of a type that the directory can contain", "other");
  add_icon (win, "folder-open", "The icon used for folders, while their contents are being displayed within the same window. This icon would normally be shown in a tree or list view, next to the main view of a folder's contents", "other");
  add_icon (win, "folder-visiting", "The icon used for folders, while their contents are being displayed in another window. This icon would typically be used when using multiple windows to navigate the hierarchy, such as in Nautilus's spatial mode", "other");
  add_icon (win, "image-loading", "The icon used when another image is being loaded, such as thumnails for larger images in the file manager", "other");
  add_icon (win, "image-missing", "The icon used when another image could not be loaded", "other");
  add_icon (win, "mail-signed", "The icon used for an electronic mail that contains a signature", "other");
  add_icon (win, "mail-signed-verified", "The icon used for an electronic mail that contains a signature which has also been verified by the security system", "other");
  add_icon (win, "network-cellular-3g", "", "other"); 
  add_icon (win, "network-cellular-4g", "", "other"); 
  add_icon (win, "network-cellular-edge", "", "other"); 
  add_icon (win, "network-cellular-gprs", "", "other"); 
  add_icon (win, "network-cellular-umts", "", "other"); 
  add_icon (win, "network-cellular-acquiring", "", "other"); 
  add_icon (win, "network-cellular-connected", "", "other"); 
  add_icon (win, "network-cellular-no-route", "", "other"); 
  add_icon (win, "network-cellular-offline", "", "other"); 
  add_icon (win, "network-cellular-signal-excellent", "", "other"); 
  add_icon (win, "network-cellular-signal-good", "", "other"); 
  add_icon (win, "network-cellular-signal-ok", "", "other"); 
  add_icon (win, "network-cellular-signal-weak", "", "other"); 
  add_icon (win, "network-cellular-signal-none", "", "other"); 
  add_icon (win, "network-vpn-acquiring", "", "other"); 
  add_icon (win, "network-vpn", "", "other"); 
  add_icon (win, "network-wired-acquiring", "", "other"); 
  add_icon (win, "network-wired-disconnected", "", "other"); 
  add_icon (win, "network-wired-no-route", "", "other"); 
  add_icon (win, "network-wired-offline", "", "other"); 
  add_icon (win, "network-wireless-acquiring", "", "other"); 
  add_icon (win, "network-wireless-connected", "", "other"); 
  add_icon (win, "network-wireless-encrypted", "", "other"); 
  add_icon (win, "network-wireless-hotspot", "", "other"); 
  add_icon (win, "network-wireless-no-route", "", "other"); 
  add_icon (win, "network-wireless-offline", "", "other"); 
  add_icon (win, "network-wireless-signal-excellent", "", "other"); 
  add_icon (win, "network-wireless-signal-good", "", "other"); 
  add_icon (win, "network-wireless-signal-ok", "", "other"); 
  add_icon (win, "network-wireless-signal-weak", "", "other"); 
  add_icon (win, "network-wireless-signal-none", "", "other"); 
  add_icon (win, "rotation-allowed", "", "other"); 
  add_icon (win, "rotation-locked", "", "other"); 
  add_icon (win, "software-update-available", "The icon used when an update is available for software installed on the computing device, through the system software update program", "other");
  add_icon (win, "software-update-urgent", "The icon used when an urgent update is available through the system software update program", "other");
  add_icon (win, "sync-error", "The icon used when an error occurs while attempting to synchronize data from the computing device, to another device", "other");
  add_icon (win, "sync-synchronizing", "The icon used while data is successfully synchronizing to another device", "other");
  add_icon (win, "touchpad-disabled", "", "other"); 
  add_icon (win, "trophy-bronze", "", "other"); 
  add_icon (win, "trophy-silver", "", "other"); 
  add_icon (win, "trophy-gold", "", "other"); 
}

static gboolean
key_press_event_cb (GtkWidget    *widget,
                    GdkEvent     *event,
                    GtkSearchBar *bar)
{
  return gtk_search_bar_handle_event (bar, event);
}

static gboolean
icon_visible_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  IconBrowserWindow *win = data;
  gchar *context;
  gchar *name;
  gint column;
  gboolean visible;

  if (win->symbolic)
    column = SYMBOLIC_NAME_COLUMN;
  else
    column = NAME_COLUMN;

  gtk_tree_model_get (model, iter,
                      column, &name,
                      CONTEXT_COLUMN, &context,
                      -1);

  visible = name != NULL && win->current_context != NULL && g_strcmp0 (context, win->current_context->id) == 0;

  g_free (name);
  g_free (context);

  return visible;
}

static void
symbolic_toggled (GtkToggleButton *toggle, IconBrowserWindow *win)
{
  gint column;

  win->symbolic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));

  if (win->symbolic)
    column = SYMBOLIC_NAME_COLUMN;
  else
    column = NAME_COLUMN;

  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (win->list), win->cell, "icon-name", column, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (win->list), win->text_cell, "text", column, NULL);
  
  gtk_tree_model_filter_refilter (win->filter_model);
  gtk_widget_queue_draw (win->list);
}

static void
icon_browser_window_init (IconBrowserWindow *win)
{
  gtk_widget_init_template (GTK_WIDGET (win));

  win->contexts = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  g_object_bind_property (win->search, "active",
                          win->searchbar, "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL);

//  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (win->list), GTK_ENTRY (win->searchentry));
  g_signal_connect (win, "key-press-event", G_CALLBACK (key_press_event_cb), win->searchbar);

  gtk_tree_model_filter_set_visible_func (win->filter_model, icon_visible_func, win, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (win->details), GTK_WINDOW (win));

  symbolic_toggled (GTK_TOGGLE_BUTTON (win->symbolic_radio), win);

  populate (win);
}

static void
icon_browser_window_class_init (IconBrowserWindowClass *class)
{
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/iconbrowser/window.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, context_list);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, filter_model);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, symbolic_radio);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, details);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, store);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, cell);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, text_cell);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, search);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, searchbar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, searchentry);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, list);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image1);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image2);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image3);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image4);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image5);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, description);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), search_text_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), selection_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), item_activated);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), selected_context_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), symbolic_toggled);
}

IconBrowserWindow *
icon_browser_window_new (IconBrowserApp *app)
{
  return g_object_new (ICON_BROWSER_WINDOW_TYPE, "application", app, NULL);
}
