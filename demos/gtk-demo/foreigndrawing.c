/* Foreign drawing
 *
 * Many applications can't use GTK+ widgets, for a variety of reasons,
 * but still want their user interface to appear integrated with the
 * rest of the desktop, and follow GTK+ themes.
 *
 * This demo shows how to use GtkStyleContext and the gtk_render_ APIs
 * to achieve this. Note that this is a very simple, non-interactive
 * example. Also note that the application is responsible for choosing
 * suitable sizes for the rendered components.
 */

#include <gtk/gtk.h>
#include <string.h>

static void
append_element (GtkWidgetPath *path,
                const char    *selector)
{
  static const struct {
    const char    *name;
    GtkStateFlags  state_flag;
  } pseudo_classes[] = {
    { "active",        GTK_STATE_FLAG_ACTIVE },
    { "hover",         GTK_STATE_FLAG_PRELIGHT },
    { "selected",      GTK_STATE_FLAG_SELECTED },
    { "disabled",      GTK_STATE_FLAG_INSENSITIVE },
    { "indeterminate", GTK_STATE_FLAG_INCONSISTENT },
    { "focus",         GTK_STATE_FLAG_FOCUSED },
    { "backdrop",      GTK_STATE_FLAG_BACKDROP },
    { "dir(ltr)",      GTK_STATE_FLAG_DIR_LTR },
    { "dir(rtl)",      GTK_STATE_FLAG_DIR_RTL },
    { "link",          GTK_STATE_FLAG_LINK },
    { "visited",       GTK_STATE_FLAG_VISITED },
    { "checked",       GTK_STATE_FLAG_CHECKED },
    { "drop(active)",  GTK_STATE_FLAG_DROP_ACTIVE }
  };
  const char *next;
  char *name;
  char type;
  guint i;

  next = strpbrk (selector, "#.:");
  if (next == NULL)
    next = selector + strlen (selector);

  name = g_strndup (selector, next - selector);
  if (g_ascii_isupper (selector[0]))
    {
      GType gtype;
      gtype = g_type_from_name (name);
      if (gtype == G_TYPE_INVALID)
        {
          g_critical ("Unknown type name `%s'", name);
          g_free (name);
          return;
        }
      gtk_widget_path_append_type (path, gtype);
    }
  else
    {
      /* Omit type, we're using name */
      gtk_widget_path_append_type (path, G_TYPE_NONE);
      gtk_widget_path_iter_set_object_name (path, -1, name);
    }
  g_free (name);

  while (*next != '\0')
    {
      type = *next;
      selector = next + 1;
      next = strpbrk (selector, "#.:");
      if (next == NULL)
        next = selector + strlen (selector);
      name = g_strndup (selector, next - selector);

      switch (type)
        {
        case '#':
          gtk_widget_path_iter_set_name (path, -1, name);
          break;

        case '.':
          gtk_widget_path_iter_add_class (path, -1, name);
          break;

        case ':':
          for (i = 0; i < G_N_ELEMENTS (pseudo_classes); i++)
            {
              if (g_str_equal (pseudo_classes[i].name, name))
                {
                  gtk_widget_path_iter_set_state (path,
                                                  -1,
                                                  gtk_widget_path_iter_get_state (path, -1)
                                                  | pseudo_classes[i].state_flag);
                  break;
                }
            }
          if (i == G_N_ELEMENTS (pseudo_classes))
            g_critical ("Unknown pseudo-class :%s", name);
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      g_free (name);
    }
}

static GtkStyleContext *
get_style (GtkStyleContext *parent,
           const char      *selector)
{
  GtkWidgetPath *path;
  GtkStyleContext *context;

  if (parent)
    path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
  else
    path = gtk_widget_path_new ();

  append_element (path, selector);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_set_parent (context, parent);
  gtk_widget_path_unref (path);

  return context;
}

static void
draw_menu (GtkWidget     *widget,
           cairo_t       *cr,
           gint           x,
           gint           y,
           gint           width,
           gint           height)
{
  GtkStyleContext *menu_context;
  GtkStyleContext *menuitem_context;
  GtkStyleContext *arrowmenuitem_context;

  /* This information is taken from the GtkMenu docs, see "CSS nodes" */
  menu_context = get_style (NULL, "menu");

  gtk_render_background (menu_context, cr, x, y, width, height);
  gtk_render_frame (menu_context, cr, x, y, width, height);

  menuitem_context = get_style (menu_context, "menuitem");
  gtk_style_context_set_state (menuitem_context, GTK_STATE_FLAG_PRELIGHT);
  gtk_render_background (menuitem_context, cr, x, y, width, 20);
  gtk_render_frame (menuitem_context, cr, x, y, width, 20);

  /* arrow for left to right */
  arrowmenuitem_context = get_style (menuitem_context, "arrow");
  gtk_style_context_set_state (arrowmenuitem_context, GTK_STATE_FLAG_DIR_LTR);
  gtk_render_arrow (arrowmenuitem_context, cr, G_PI / 2, x + width - 20, y, 20);

  gtk_render_background (menuitem_context, cr, x, y + 20, width, 20);
  gtk_render_frame (menuitem_context, cr, x, y + 20, width, 20);

  /* arrow for right to left */
  gtk_style_context_get_state (arrowmenuitem_context);
  gtk_style_context_set_state (arrowmenuitem_context, GTK_STATE_FLAG_DIR_RTL);
  gtk_render_arrow (arrowmenuitem_context, cr, G_PI / 2, x, y + 20, 20);

  g_object_unref (arrowmenuitem_context);
  g_object_unref (menuitem_context);
  g_object_unref (menu_context);
}

static void
draw_notebook (GtkWidget     *widget,
               cairo_t       *cr,
               gint           x,
               gint           y,
               gint           width,
               gint           height)
{
  GtkStyleContext *notebook_context;
  GtkStyleContext *stack_context;
  GtkStyleContext *header_context;
  GtkStyleContext *tabs_context;
  GtkStyleContext *tab_context;

  /* This information is taken from the GtkNotebook docs, see "CSS nodes" */
  notebook_context = get_style (NULL, "notebook.frame");

  gtk_render_background (notebook_context, cr, x, y, width, height);
  gtk_render_frame (notebook_context, cr, x, y, width, height);

  stack_context = get_style (notebook_context, "stack");
  gtk_render_background (stack_context, cr, x, y + 30, width, height - 30);
  gtk_render_frame (stack_context, cr, x, y + 30, width, height - 30);

  header_context = get_style (notebook_context, "header.frame");
  gtk_render_background (header_context, cr, x, y, width, 30);
  gtk_render_frame (header_context, cr, x, y, width, 30);

  tabs_context = get_style (header_context, "tabs");
  gtk_render_background (tabs_context, cr, x, y, width, 30);
  gtk_render_frame (tabs_context, cr, x, y, width, 30);

  /* what do I need to do to get the box-shadow rendered ? */
  tab_context = get_style (tabs_context, "tab:active");
  gtk_render_background (tab_context, cr, x, y, 50, 30);
  gtk_render_frame (tab_context, cr, x, y, 50, 30);

  g_object_unref (tab_context);
  g_object_unref (tabs_context);
  g_object_unref (header_context);
  g_object_unref (stack_context);
  g_object_unref (notebook_context);
}


static void
draw_horizontal_scrollbar (GtkWidget     *widget,
                           cairo_t       *cr,
                           gint           x,
                           gint           y,
                           gint           width,
                           gint           height,
                           gint           position,
                           GtkStateFlags  state)
{
  GtkStyleContext *scrollbar_context;
  GtkStyleContext *trough_context;
  GtkStyleContext *slider_context;

  /* This information is taken from the GtkScrollbar docs, see "CSS nodes" */
  scrollbar_context = get_style (NULL, "scrollbar.horizontal");
  trough_context = get_style (scrollbar_context, "trough");
  slider_context = get_style (trough_context, "slider");

  gtk_style_context_set_state (scrollbar_context, state);
  gtk_style_context_set_state (trough_context, state);
  gtk_style_context_set_state (slider_context, state);

  gtk_render_background (trough_context, cr, x, y, width, height);
  gtk_render_frame (trough_context, cr, x, y, width, height);
  gtk_render_slider (slider_context, cr, x + position, y + 1, 30, height - 2, GTK_ORIENTATION_HORIZONTAL);

  g_object_unref (slider_context);
  g_object_unref (trough_context);
  g_object_unref (scrollbar_context);
}

static void
draw_text (GtkWidget     *widget,
           cairo_t       *cr,
           gint           x,
           gint           y,
           gint           width,
           gint           height,
           const gchar   *text,
           GtkStateFlags  state)
{
  GtkStyleContext *label_context;
  GtkStyleContext *selection_context;
  GtkStyleContext *context;
  PangoLayout *layout;

  /* This information is taken from the GtkLabel docs, see "CSS nodes" */
  label_context = get_style (NULL, "label.view");
  selection_context = get_style (label_context, "selection");

  gtk_style_context_set_state (label_context, state);

  if (state & GTK_STATE_FLAG_SELECTED)
    context = selection_context;
  else
    context = label_context;

  layout = gtk_widget_create_pango_layout (widget, text);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);
  gtk_render_layout (context, cr, x, y, layout);

  g_object_unref (layout);

  g_object_unref (selection_context);
  g_object_unref (label_context);
}

static void
draw_check (GtkWidget     *widget,
            cairo_t       *cr,
            gint           x,
            gint           y,
            GtkStateFlags  state)
{
  GtkStyleContext *button_context;
  GtkStyleContext *check_context;

  /* This information is taken from the GtkCheckButton docs, see "CSS nodes" */
  button_context = get_style (NULL, "checkbutton");
  check_context = get_style (button_context, "check");

  gtk_style_context_set_state (check_context, state);

  gtk_render_background (check_context, cr, x, y, 20, 20);
  gtk_render_frame (check_context, cr, x, y, 20, 20);
  gtk_render_check (check_context, cr, x, y, 20, 20);

  g_object_unref (check_context);
  g_object_unref (button_context);

}

static void
draw_radio (GtkWidget     *widget,
            cairo_t       *cr,
            gint           x,
            gint           y,
            GtkStateFlags  state)
{
  GtkStyleContext *button_context;
  GtkStyleContext *check_context;

  /* This information is taken from the GtkRadioButton docs, see "CSS nodes" */
  button_context = get_style (NULL, "radiobutton");
  check_context = get_style (button_context, "radio");

  gtk_style_context_set_state (check_context, state);

  gtk_render_background (check_context, cr, x, y, 20, 20);
  gtk_render_frame (check_context, cr, x, y, 20, 20);
  gtk_render_option (check_context, cr, x, y, 20, 20);

  g_object_unref (check_context);
  g_object_unref (button_context);

}

static void
draw_progress (GtkWidget *widget,
               cairo_t   *cr,
               gint       x,
               gint       y,
               gint       width,
               gint       height,
               gint       position)
{
  GtkStyleContext *bar_context;
  GtkStyleContext *trough_context;
  GtkStyleContext *progress_context;

  /* This information is taken from the GtkProgressBar docs, see "CSS nodes" */
  bar_context = get_style (NULL, "progressbar");
  trough_context = get_style (bar_context, "trough");
  progress_context = get_style (trough_context, "progress");

  gtk_render_background (trough_context, cr, x, y, width, height);
  gtk_render_frame (trough_context, cr, x, y, width, height);
  gtk_render_background (progress_context, cr, x, y, position, height);
  gtk_render_frame (progress_context, cr, x, y, position, height);

  g_object_unref (progress_context);
  g_object_unref (trough_context);
  g_object_unref (bar_context);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr)
{
  gint panewidth, width, height;

  width = gtk_widget_get_allocated_width (widget);
  panewidth = width / 2;
  height = gtk_widget_get_allocated_height (widget);

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);

  draw_horizontal_scrollbar (widget, cr, 10, 10, panewidth - 20, 10, 30, GTK_STATE_FLAG_NORMAL);
  draw_horizontal_scrollbar (widget, cr, 10, 30, panewidth - 20, 10, 40, GTK_STATE_FLAG_PRELIGHT);
  draw_horizontal_scrollbar (widget, cr, 10, 50, panewidth - 20, 10, 50, GTK_STATE_FLAG_ACTIVE|GTK_STATE_FLAG_PRELIGHT);

  draw_text (widget, cr, 10,  70, panewidth - 20, 20, "Not selected", GTK_STATE_FLAG_NORMAL);
  draw_text (widget, cr, 10, 100, panewidth - 20, 20, "Selected", GTK_STATE_FLAG_SELECTED);

  draw_check (widget, cr,  10, 130, GTK_STATE_FLAG_NORMAL);
  draw_check (widget, cr,  40, 130, GTK_STATE_FLAG_CHECKED);
  draw_radio (widget, cr,  70, 130, GTK_STATE_FLAG_NORMAL);
  draw_radio (widget, cr, 100, 130, GTK_STATE_FLAG_CHECKED);
  draw_progress (widget, cr, 10, 160, panewidth - 20, 6, 50);

  draw_menu (widget, cr, 10 + panewidth, 10, panewidth - 20, 160);

  draw_notebook (widget, cr, 10, 200, panewidth - 20, 160);

  return FALSE;
}

GtkWidget *
do_foreigndrawing (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box;
      GtkWidget *da;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Foreign drawing");
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_add (GTK_CONTAINER (window), box);
      da = gtk_drawing_area_new ();
      gtk_widget_set_size_request (da, 400, 400);
      gtk_widget_set_hexpand (da, TRUE);
      gtk_widget_set_vexpand (da, TRUE);
      gtk_widget_set_app_paintable (da, TRUE);
      gtk_container_add (GTK_CONTAINER (box), da);

      g_signal_connect (da, "draw", G_CALLBACK (draw_cb), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
