/* Gtk+ default value tests
 * Copyright (C) 2007 Christian Persch
 *               2007 Johan Dahlin
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

#undef GTK_DISABLE_DEPRECATED
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtktypebuiltins.h>
#include <gtk/gtkprinter.h>
#include <gtk/gtkpagesetupunixdialog.h>
#include <gtk/gtkprintunixdialog.h>

typedef GType (* GetTypeFunc) (void);

static const char *types[] = {
"gtk_about_dialog_get_type",
"gtk_accel_flags_get_type",
"gtk_accel_group_get_type",
"gtk_accel_label_get_type",
"gtk_accel_map_get_type",
"gtk_accessible_get_type",
"gtk_action_get_type",
"gtk_action_group_get_type",
"gtk_adjustment_get_type",
"gtk_alignment_get_type",
"gtk_anchor_type_get_type",
"gtk_arg_flags_get_type",
"gtk_arrow_get_type",
"gtk_arrow_type_get_type",
"gtk_aspect_frame_get_type",
"gtk_assistant_get_type",
"gtk_assistant_page_type_get_type",
"gtk_attach_options_get_type",
"gtk_bin_get_type",
"gtk_border_get_type",
"gtk_box_get_type",
"gtk_buildable_get_type",
"gtk_builder_error_get_type",
"gtk_builder_get_type",
"gtk_button_action_get_type",
"gtk_button_box_get_type",
"gtk_button_box_style_get_type",
"gtk_button_get_type",
"gtk_buttons_type_get_type",
"gtk_calendar_display_options_get_type",
"gtk_calendar_get_type",
"gtk_cell_editable_get_type",
"gtk_cell_layout_get_type",
"gtk_cell_renderer_accel_get_type",
"gtk_cell_renderer_accel_mode_get_type",
"gtk_cell_renderer_combo_get_type",
"gtk_cell_renderer_get_type",
"gtk_cell_renderer_mode_get_type",
"gtk_cell_renderer_pixbuf_get_type",
"gtk_cell_renderer_progress_get_type",
"gtk_cell_renderer_spin_get_type",
"gtk_cell_renderer_state_get_type",
"gtk_cell_renderer_text_get_type",
"gtk_cell_renderer_toggle_get_type",
"gtk_cell_type_get_type",
"gtk_cell_view_get_type",
"gtk_check_button_get_type",
"gtk_check_menu_item_get_type",
"gtk_clipboard_get_type",
"gtk_clist_drag_pos_get_type",
"gtk_clist_get_type", 
"gtk_color_button_get_type",
"gtk_color_selection_dialog_get_type",
"gtk_color_selection_get_type",
"gtk_combo_box_entry_get_type",
"gtk_combo_box_get_type",
"gtk_combo_get_type",
"gtk_container_get_type",
"gtk_corner_type_get_type",
"gtk_ctree_expander_style_get_type",
"gtk_ctree_expansion_type_get_type",
"gtk_ctree_get_type",
"gtk_ctree_line_style_get_type",
"gtk_ctree_node_get_type",
"gtk_ctree_pos_get_type",
"gtk_curve_get_type",
"gtk_curve_type_get_type",
"gtk_debug_flag_get_type",
"gtk_delete_type_get_type",
"gtk_dest_defaults_get_type",
"gtk_dialog_flags_get_type",
"gtk_dialog_get_type",
"gtk_direction_type_get_type",
"gtk_drag_result_get_type",
"gtk_drawing_area_get_type",
"gtk_editable_get_type",
"gtk_entry_completion_get_type",
"gtk_entry_get_type",
"gtk_event_box_get_type",
"gtk_expander_get_type",
"gtk_expander_style_get_type",
"gtk_file_chooser_action_get_type",
"gtk_file_chooser_button_get_type",
"gtk_file_chooser_confirmation_get_type",
"gtk_file_chooser_dialog_get_type",
"gtk_file_chooser_error_get_type",
"gtk_file_chooser_get_type",
"gtk_file_chooser_widget_get_type",
"gtk_file_filter_flags_get_type",
"gtk_file_filter_get_type",
"gtk_file_folder_get_type",
"gtk_file_info_get_type",
"gtk_file_path_get_type",
"gtk_file_selection_get_type",
"gtk_file_system_get_type",
"gtk_file_system_handle_get_type",
"gtk_file_system_unix_get_type",
"gtk_fixed_get_type",
"gtk_font_button_get_type",
"gtk_font_selection_dialog_get_type",
"gtk_font_selection_get_type",
"gtk_frame_get_type",
"gtk_gamma_curve_get_type",
"gtk_handle_box_get_type",
"gtk_hbox_get_type",
"gtk_hbutton_box_get_type",
"gtk_hpaned_get_type",
"gtk_hruler_get_type",
"gtk_hscale_get_type",
"gtk_hscrollbar_get_type",
"gtk_hseparator_get_type",
"gtk_hsv_get_type",
"gtk_icon_factory_get_type",
"gtk_icon_info_get_type",
"gtk_icon_lookup_flags_get_type",
"gtk_icon_set_get_type",
"gtk_icon_size_get_type",
"gtk_icon_source_get_type",
"gtk_icon_theme_error_get_type",
"gtk_icon_theme_get_type",
"gtk_icon_view_drop_position_get_type",
"gtk_icon_view_get_type",
"gtk_identifier_get_type",
"gtk_image_get_type",
"gtk_image_menu_item_get_type",
"gtk_image_type_get_type",
"gtk_im_context_get_type",
"gtk_im_context_simple_get_type",
"gtk_im_multicontext_get_type",
"gtk_im_preedit_style_get_type",
"gtk_im_status_style_get_type",
"gtk_input_dialog_get_type",
"gtk_invisible_get_type",
"gtk_item_factory_get_type",
"gtk_item_get_type",
"gtk_justification_get_type",
"gtk_label_get_type",
"gtk_layout_get_type",
"gtk_link_button_get_type",
"gtk_list_get_type",
"gtk_list_item_get_type",
"gtk_list_store_get_type",
"gtk_match_type_get_type",
"gtk_menu_bar_get_type",
"gtk_menu_direction_type_get_type",
"gtk_menu_get_type",
"gtk_menu_item_get_type",
"gtk_menu_shell_get_type",
"gtk_menu_tool_button_get_type",
"gtk_message_dialog_get_type",
"gtk_message_type_get_type",
"gtk_metric_type_get_type",
"gtk_misc_get_type",
"gtk_movement_step_get_type",
"gtk_notebook_get_type",
"gtk_notebook_tab_get_type",
"gtk_object_flags_get_type",
"gtk_object_get_type",
"gtk_old_editable_get_type",
"gtk_option_menu_get_type",
"gtk_orientation_get_type",
"gtk_pack_direction_get_type",
"gtk_pack_type_get_type",
"gtk_page_orientation_get_type",
"gtk_page_set_get_type",
"gtk_page_setup_get_type",
"gtk_page_setup_unix_dialog_get_type",
"gtk_paned_get_type",
"gtk_paper_size_get_type",
"gtk_path_bar_get_type",
"gtk_path_priority_type_get_type",
"gtk_path_type_get_type",
"gtk_pixmap_get_type",
"gtk_plug_get_type",
"gtk_policy_type_get_type",
"gtk_position_type_get_type",
"gtk_preview_get_type",
"gtk_preview_type_get_type",
"gtk_print_backend_get_type",
"gtk_print_capabilities_get_type",
"gtk_print_context_get_type",
"gtk_print_duplex_get_type",
"gtk_printer_get_type",
"gtk_printer_option_get_type",
"gtk_printer_option_set_get_type",
"gtk_printer_option_widget_get_type",
"gtk_print_error_get_type",
"gtk_print_job_get_type",
"gtk_print_operation_action_get_type",
"gtk_print_operation_get_type",
"gtk_print_operation_preview_get_type",
"gtk_print_operation_result_get_type",
"gtk_print_pages_get_type",
"gtk_print_quality_get_type",
"gtk_print_settings_get_type",
"gtk_print_status_get_type",
"gtk_print_unix_dialog_get_type",
"gtk_private_flags_get_type",
"gtk_progress_bar_get_type",
"gtk_progress_bar_orientation_get_type",
"gtk_progress_bar_style_get_type",
"gtk_progress_get_type",
"gtk_radio_action_get_type",
"gtk_radio_button_get_type",
"gtk_radio_menu_item_get_type",
"gtk_radio_tool_button_get_type",
"gtk_range_get_type",
"gtk_rc_flags_get_type",
"gtk_rc_style_get_type",
"gtk_rc_token_type_get_type",
"gtk_recent_action_get_type",
"gtk_recent_chooser_dialog_get_type",
"gtk_recent_chooser_error_get_type",
"gtk_recent_chooser_get_type",
"gtk_recent_chooser_menu_get_type",
"gtk_recent_chooser_widget_get_type",
"gtk_recent_filter_flags_get_type",
"gtk_recent_filter_get_type",
"gtk_recent_info_get_type",
"gtk_recent_manager_error_get_type",
"gtk_recent_manager_get_type",
"gtk_recent_sort_type_get_type",
"gtk_relief_style_get_type",
"gtk_requisition_get_type",
"gtk_resize_mode_get_type",
"gtk_response_type_get_type",
"gtk_ruler_get_type",
"gtk_scale_button_get_type",
"gtk_scale_get_type",
"gtk_scrollbar_get_type",
"gtk_scrolled_window_get_type",
"gtk_scroll_step_get_type",
"gtk_scroll_type_get_type",
"gtk_selection_data_get_type",
"gtk_selection_mode_get_type",
"gtk_sensitivity_type_get_type",
"gtk_separator_get_type",
"gtk_separator_menu_item_get_type",
"gtk_separator_tool_item_get_type",
"gtk_settings_get_type",
"gtk_shadow_type_get_type",
"gtk_side_type_get_type",
"gtk_signal_run_type_get_type",
"gtk_size_group_get_type",
"gtk_size_group_mode_get_type",
"gtk_socket_get_type",
"gtk_sort_type_get_type",
"gtk_spin_button_get_type",
"gtk_spin_button_update_policy_get_type",
"gtk_spin_type_get_type",
"gtk_state_type_get_type",
"gtk_statusbar_get_type",
"gtk_status_icon_get_type",
"gtk_style_get_type",
"gtk_submenu_direction_get_type",
"gtk_submenu_placement_get_type",
"gtk_table_get_type",
"gtk_target_flags_get_type",
"gtk_target_list_get_type",
"gtk_tearoff_menu_item_get_type",
"gtk_text_attributes_get_type",
"gtk_text_buffer_get_type",
"gtk_text_buffer_target_info_get_type",
"gtk_text_child_anchor_get_type",
"gtk_text_direction_get_type",
"gtk_text_iter_get_type",
"gtk_text_layout_get_type",
"gtk_text_mark_get_type",
"gtk_text_search_flags_get_type",
"gtk_text_tag_get_type",
"gtk_text_tag_table_get_type",
"gtk_text_view_get_type",
"gtk_text_window_type_get_type",
"gtk_theme_engine_get_type",
"gtk_tips_query_get_type",
"gtk_toggle_action_get_type",
"gtk_toggle_button_get_type",
"gtk_toggle_tool_button_get_type",
"gtk_toolbar_child_type_get_type",
"gtk_toolbar_get_type",
"gtk_toolbar_space_style_get_type",
"gtk_toolbar_style_get_type",
"gtk_tool_button_get_type",
"gtk_tool_item_get_type",
"gtk_tooltip_get_type",
"gtk_tooltips_get_type",
"gtk_tray_icon_get_type",
"gtk_tree_drag_dest_get_type",
"gtk_tree_drag_source_get_type",
"gtk_tree_iter_get_type",
"gtk_tree_model_filter_get_type",
"gtk_tree_model_flags_get_type",
"gtk_tree_model_get_type",
"gtk_tree_model_sort_get_type",
"gtk_tree_path_get_type",
"gtk_tree_row_reference_get_type",
"gtk_tree_selection_get_type",
"gtk_tree_sortable_get_type",
"gtk_tree_store_get_type",
"gtk_tree_view_column_get_type",
"gtk_tree_view_column_sizing_get_type",
"gtk_tree_view_drop_position_get_type",
"gtk_tree_view_get_type",
"gtk_tree_view_grid_lines_get_type",
"gtk_tree_view_mode_get_type",
"gtk_ui_manager_get_type",
"gtk_ui_manager_item_type_get_type",
"gtk_unit_get_type",
"gtk_update_type_get_type",
"gtk_vbox_get_type",
"gtk_vbutton_box_get_type",
"gtk_viewport_get_type",
"gtk_visibility_get_type",
"gtk_volume_button_get_type",
"gtk_vpaned_get_type",
"gtk_vruler_get_type",
"gtk_vscale_get_type",
"gtk_vscrollbar_get_type",
"gtk_vseparator_get_type",
"gtk_widget_flags_get_type",
"gtk_widget_get_type",
"gtk_widget_help_type_get_type",
"gtk_window_get_type",
"gtk_window_group_get_type",
"gtk_window_position_get_type",
"gtk_window_type_get_type",
"gtk_wrap_mode_get_type",
};

static void
check_property (const char *output,
	        GParamSpec *pspec,
		GValue *value)
{
  GValue default_value = { 0, };
  char *v, *dv, *msg;

  if (g_param_value_defaults (pspec, value))
      return;

  g_value_init (&default_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_param_value_set_default (pspec, &default_value);
      
  v = g_strdup_value_contents (value);
  dv = g_strdup_value_contents (&default_value);
  
  msg = g_strdup_printf ("%s %s.%s: %s != %s\n",
			 output,
			 g_type_name (pspec->owner_type),
			 pspec->name,
			 dv, v);
  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__,
		       G_STRFUNC, msg);
  g_free (msg);
  
  g_free (v);
  g_free (dv);
  g_value_unset (&default_value);
}

static void
test_type (gconstpointer data)
{
  GObjectClass *klass;
  GObject *instance;
  GParamSpec **pspecs;
  guint n_pspecs, i;
  GType type;
  
  type = GPOINTER_TO_INT (data);
  
  if (!G_TYPE_IS_CLASSED (type))
    return;

  if (G_TYPE_IS_ABSTRACT (type))
    return;

  if (!g_type_is_a (type, G_TYPE_OBJECT))
    return;

  /* These can't be freely constructed/destroyed */
  if (g_type_is_a (type, GTK_TYPE_PRINT_JOB))
    return;

  /* The gtk_arg compat wrappers can't set up default values */
  if (g_type_is_a (type, GTK_TYPE_CLIST) ||
      g_type_is_a (type, GTK_TYPE_CTREE) ||
      g_type_is_a (type, GTK_TYPE_LIST) ||
      g_type_is_a (type, GTK_TYPE_TIPS_QUERY))
    return;

  klass = g_type_class_ref (type);
  
  if (g_type_is_a (type, GTK_TYPE_SETTINGS))
    instance = g_object_ref (gtk_settings_get_default ());
  else
    instance = g_object_new (type, NULL);

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue value = { 0, };
      
      if (pspec->owner_type != type)
	continue;

      if ((pspec->flags & G_PARAM_READABLE) == 0)
	continue;

      /* Filter these out */
      if (g_type_is_a (type, GTK_TYPE_WIDGET) &&
	  (strcmp (pspec->name, "name") == 0 ||
	   strcmp (pspec->name, "screen") == 0 ||
	   strcmp (pspec->name, "style") == 0))
	continue;

      /* These are set to the current date */
      if (g_type_is_a (type, GTK_TYPE_CALENDAR) &&
	  (strcmp (pspec->name, "year") == 0 ||
	   strcmp (pspec->name, "month") == 0 ||
	   strcmp (pspec->name, "day") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_RENDERER_TEXT) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_VIEW) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_BUTTON) &&
	  strcmp (pspec->name, "color") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_SELECTION) &&
	  strcmp (pspec->name, "current-color") == 0)
	continue;

      /* Gets set to the cwd */
      if (g_type_is_a (type, GTK_TYPE_FILE_SELECTION) &&
	  strcmp (pspec->name, "filename") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_FONT_SELECTION) &&
	  strcmp (pspec->name, "font") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_LAYOUT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_MESSAGE_DIALOG) &&
	  strcmp (pspec->name, "image") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_PRINT_OPERATION) &&
	  strcmp (pspec->name, "job-name") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_PRINT_UNIX_DIALOG) &&
	  (strcmp (pspec->name, "page-setup") == 0 ||
	   strcmp (pspec->name, "print-settings") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_PROGRESS_BAR) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      /* filename value depends on $HOME */
      if (g_type_is_a (type, GTK_TYPE_RECENT_MANAGER) &&
          strcmp (pspec->name, "filename") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SCALE_BUTTON) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SCROLLED_WINDOW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      /* these defaults come from XResources */
      if (g_type_is_a (type, GTK_TYPE_SETTINGS) &&
          strncmp (pspec->name, "gtk-xft-", 8) == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SETTINGS) &&
          strcmp (pspec->name, "color-hash") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SPIN_BUTTON) &&
          (strcmp (pspec->name, "adjustment") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_STATUS_ICON) &&
          (strcmp (pspec->name, "size") == 0 ||
           strcmp (pspec->name, "screen") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT_BUFFER) &&
          (strcmp (pspec->name, "tag-table") == 0 ||
           strcmp (pspec->name, "copy-target-list") == 0 ||
           strcmp (pspec->name, "paste-target-list") == 0))
        continue;

      /* language depends on the current locale */
      if (g_type_is_a (type, GTK_TYPE_TEXT_TAG) &&
          (strcmp (pspec->name, "background-gdk") == 0 ||
           strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "language") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT_VIEW) &&
          strcmp (pspec->name, "buffer") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_VIEWPORT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (instance, pspec->name, &value);
      check_property ("Property", pspec, &value);
      g_value_unset (&value);
    }

  if (g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      pspecs = gtk_widget_class_list_style_properties (GTK_WIDGET_CLASS (klass), &n_pspecs);
      
      for (i = 0; i < n_pspecs; ++i)
	{
	  GParamSpec *pspec = pspecs[i];
	  GValue value = { 0, };
	  
	  if (pspec->owner_type != type)
	    continue;

	  if ((pspec->flags & G_PARAM_READABLE) == 0)
	    continue;
	  
	  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  gtk_widget_style_get_property (GTK_WIDGET (instance), pspec->name, &value);
	  check_property ("Style property", pspec, &value);
	  g_value_unset (&value);
	}
    }
  
  g_object_unref (instance);
  g_type_class_unref (klass);
}

int
main (int argc, char **argv)
{
  GModule *module;
  guint i;

  gtk_test_init (&argc, &argv);
  
  module = g_module_open (NULL, 0);
  if (!module)
    return 1;

  /* GtkAboutDialog:program-name workaround */
  g_set_prgname (NULL);

  for (i = 0; i < G_N_ELEMENTS (types); ++i)
    {
      GetTypeFunc func;
      gpointer ptr = &func;
      GType type;
      gchar *testname;
      
      if (!g_module_symbol (module, types[i], ptr))
	g_assert_not_reached ();
      
      type = func ();

      g_assert (type != G_TYPE_INVALID);

      testname = g_strdup_printf ("/Default Values/%s",
				  g_type_name (type));
      g_test_add_data_func (testname,
			    GINT_TO_POINTER (type),
			    test_type);
      g_free (testname);
    }
  
  return g_test_run();
}
