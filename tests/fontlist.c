#include <gtk/gtk.h>

static gpointer
pick_one_face (gpointer item, gpointer user_data)
{
  PangoFontFamily *family = PANGO_FONT_FAMILY (item);
  PangoFontFace *face;

  face = pango_font_family_get_face (family, "Regular");
  
  g_object_unref (family);

  return g_object_ref (face);
}

int
main (int argc, char *argv[])
{
  PangoFontMap *fontmap;
  int i;
  GListModel *model;

  fontmap = pango_cairo_font_map_get_default ();
  model = G_LIST_MODEL (fontmap);
  g_print ("Families\n--------\n");
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      PangoFontFamily *family = PANGO_FONT_FAMILY (g_list_model_get_item (model, i));

      g_print ("%s\n", pango_font_family_get_name (family));

      g_object_unref (family);
    }
 
  g_print ("All faces\n-----\n");
  model = G_LIST_MODEL (gtk_flatten_list_model_new (PANGO_TYPE_FONT_FACE, model));
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      PangoFontFace *face = PANGO_FONT_FACE (g_list_model_get_item (model, i));

      g_print ("%s %s\n",
               pango_font_family_get_name (pango_font_face_get_family (face)),
               pango_font_face_get_face_name (face));

      g_object_unref (face);
    }
  g_object_unref (model);

  g_print ("One face per family\n-------------------\n");
  model = G_LIST_MODEL (gtk_map_list_model_new (PANGO_TYPE_FONT_FACE,
                                                G_LIST_MODEL (fontmap),
                                                pick_one_face,
                                                NULL, NULL));
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      PangoFontFace *face = PANGO_FONT_FACE (g_list_model_get_item (model, i));

      g_print ("%s %s\n",
               pango_font_family_get_name (pango_font_face_get_family (face)),
               pango_font_face_get_face_name (face));

      g_object_unref (face);
    }
  g_object_unref (model);

  g_object_unref (fontmap);

  return 0;
}
