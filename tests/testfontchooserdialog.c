/* testfontchooserdialog.c
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <pango/pangofc-fontmap.h>
#include <gtk/gtk.h>

static gboolean
monospace_filter (const PangoFontFamily *family,
                  const PangoFontFace   *face,
                  gpointer               data)
{
  return pango_font_family_is_monospace ((PangoFontFamily *) family);
}

static void
notify_font_cb (GtkFontChooser *fontchooser, GParamSpec *pspec, gpointer data)
{
  PangoFontFamily *family;
  PangoFontFace *face;

  g_debug ("Changed font name %s", gtk_font_chooser_get_font (fontchooser));

  family = gtk_font_chooser_get_font_family (fontchooser);
  face = gtk_font_chooser_get_font_face (fontchooser);
  if (family)
    {
       g_debug ("  Family: %s is-monospace:%s",
                pango_font_family_get_name (family),
                pango_font_family_is_monospace (family) ? "true" : "false");
    }
  else
    g_debug ("  No font family!");

  if (face)
    g_debug ("  Face description: %s", pango_font_face_get_face_name (face));
  else
    g_debug ("  No font face!");
}

static void
notify_preview_text_cb (GObject *fontchooser, GParamSpec *pspec, gpointer data)
{
  g_debug ("Changed preview text %s", gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (fontchooser)));
}

static void
font_activated_cb (GtkFontChooser *chooser, const gchar *font_name, gpointer data)
{
  g_debug ("font-activated: %s", font_name);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *font_button;

  gtk_init (&argc, &argv);

  font_button = gtk_font_button_new ();

  if (argc > 0)
    {
      FcConfig *config;
      PangoFontMap *fontmap;
      gint i;

      /* Create a custom font configuration by adding font files specified
       * on the commandline to the default config.
       */
      config = FcInitLoadConfigAndFonts ();
      for (i = 0; i < argc; i++)
        FcConfigAppFontAddFile (config, (const FcChar8 *)argv[i]);

      fontmap = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
      pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (fontmap), config);
      gtk_font_chooser_set_font_map (GTK_FONT_CHOOSER (font_button), fontmap);
    }

  gtk_font_button_set_use_font (GTK_FONT_BUTTON (font_button), TRUE);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), font_button);
  gtk_widget_show_all (window);

  g_signal_connect (font_button, "notify::font",
                    G_CALLBACK (notify_font_cb), NULL);
  g_signal_connect (font_button, "notify::preview-text",
                    G_CALLBACK (notify_preview_text_cb), NULL);
  g_signal_connect (font_button, "font-activated",
                    G_CALLBACK (font_activated_cb), NULL);

  if (argc >= 2 && strcmp (argv[1], "--monospace") == 0)
    {
      gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (font_button),
                                        monospace_filter, NULL, NULL);
    }

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
