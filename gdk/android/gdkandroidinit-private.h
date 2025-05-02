/*
 * Copyright (c) 2024-2025 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gdkandroidinit.h"

G_BEGIN_DECLS

typedef struct _GdkAndroidObject GdkAndroidObject;

typedef struct
{
  struct
  {
    jclass klass;
    jmethodID constructor;
    jfieldID native_ptr;
  } clipboard_provider_change_listener;
  struct
  {
    jclass klass;
    jmethodID constructor;
    jmethodID vflip;
  } clipboard_bitmap_drag_shadow;
  struct
  {
    jclass klass;
    jmethodID constructor;
  } clipboard_empty_drag_shadow;
  struct {
    jclass klass;
    jmethodID constructor;
  } clipboard_internal_clipdata;
  struct {
    jclass klass;
    jmethodID constructor;
    jfieldID native_identifier;
  } clipboard_native_drag_identifier;
  struct
  {
    jclass klass;
    jfieldID surface_identifier;
    jmethodID get_holder;
    jmethodID set_ime_keyboard_state;
    jmethodID set_visibility;
    jmethodID set_input_region;
    jmethodID drop_cursor_icon;
    jmethodID set_cursor_from_id;
    jmethodID set_cursor_from_bitmap;
    jmethodID start_dnd;
    jmethodID update_dnd;
    jmethodID cancel_dnd;
    jmethodID set_active_im_context;
    jmethodID reposition;
    jmethodID drop;
  } surface;
  struct
  {
    jclass klass;
    jstring toplevel_identifier_key;
    jfieldID native_identifier;
    jfieldID toplevel_view;
    jmethodID bind_native;
    jmethodID attach_toplevel_surface;
    jmethodID post_window_configuration;
    jmethodID post_title;
  } toplevel;
  struct
  {
    jclass klass;
    jmethodID set_grabbed_surface;
    jmethodID push_popup;
  } toplevel_view;
  struct
  {
    jclass klass;
    jmethodID constructor;
  } surface_exception;
  struct
  {
    jclass klass;
    jmethodID get_task_id;
    jmethodID get_window_manager;
    jmethodID finish;
    jmethodID move_task_to_back;
    jmethodID start_activity;
    jmethodID start_activity_for_result;
    jmethodID finish_activity;
    jmethodID set_finish_on_touch_outside;
    jint result_ok;
    jint result_cancelled;
  } a_activity;
  struct
  {
    jclass klass;
    jmethodID get_content_resolver;
    jmethodID get_system_service;
    jmethodID get_resources;
    jstring activity_service;
    jstring clipboard_service;
  } a_context;
  struct
  {
    jclass klass;
    jmethodID get_type;
    jmethodID open_asset_fd;
    jmethodID open_typed_asset_fd;
    jmethodID query;
  } a_content_resolver;
  struct
  {
    jclass klass;
    jmethodID create_istream;
    jmethodID create_ostream;
    jstring mode_append;
    jstring mode_read;
    jstring mode_overwrite;
  } a_asset_fd;
  struct
  {
    jclass klass;
    jmethodID get_document_id;
    jmethodID get_tree_document_id;
    jmethodID build_children_from_tree;
    jmethodID build_document_from_tree;
    jmethodID copy_document;
    jmethodID create_document;
    jmethodID delete_document;
    jmethodID is_child_document;
    jmethodID is_document;
    jmethodID is_tree;
    jmethodID rename_document;
  } a_documents_contract;
  struct
  {
    jclass klass;
    jstring column_document_id;
    jstring column_display_name;
    jstring column_flags;
    jstring column_icon;
    jstring column_last_modified;
    jstring column_mime_type;
    jstring column_size;
    jstring column_summary;
    jint flag_dir_supports_create;
    jint flag_supports_copy;
    jint flag_supports_delete;
    jint flag_supports_move;
    jint flag_supports_rename;
    jint flag_supports_write;
    jint flag_virtual_document;
    jstring mime_directory;
  } a_documents_contract_document;
  struct
  {
    jclass klass;
    jmethodID get_int;
    jmethodID get_long;
    jmethodID get_string;
    jmethodID is_null;
    jmethodID move_to_next;
    jmethodID close;
  } a_cursor;
  struct
  {
    jclass klass;
    jmethodID get_configuration;
  } a_resources;
  struct
  {
    jclass klass;
    jfieldID ui;
    jint ui_night_undefined;
    jint ui_night_no;
    jint ui_night_yes;
  } a_configuration;
  struct
  {
    jclass klass;
    jmethodID get_primary_clip;
    jmethodID set_primary_clip;
    jmethodID get_clip_desc;
    jmethodID add_change_listener;
    jmethodID remove_change_listener;
  } a_clipboard_manager;
  struct
  {
    jclass klass;
    jmethodID get_mime_type_count;
    jmethodID get_mime_type;
    jobject mime_text_html;
    jobject mime_text_plain;
  } a_clip_desc;
  struct
  {
    jclass klass;
    jmethodID add_item;
    jmethodID get_item_count;
    jmethodID get_item;
    jmethodID new_plain_text;
    jmethodID new_html;
    jmethodID new_uri;
  } a_clipdata;
  struct
  {
    jclass klass;
    jmethodID constructor_text;
    jmethodID constructor_html;
    jmethodID constructor_uri;
    jmethodID coerce_to_text;
    jmethodID get_html;
    jmethodID get_uri;
  } a_clipdata_item;
  struct
  {
    jclass klass;
    jmethodID get_context;
    jmethodID get_display;
    jint drag_global;
    jint drag_global_prefix_match;
    jint drag_global_uri_read;
  } a_view;
  struct
  {
    jclass klass;
    jmethodID get_refresh_rate;
  } a_display;
  struct
  {
    jclass klass;
    jint type_alias;
    jint type_all_scroll;
    jint type_arrow;
    jint type_cell;
    jint type_context_menu;
    jint type_copy;
    jint type_crosshair;
    jint type_grab;
    jint type_grabbing;
    jint type_hand;
    jint type_help;
    jint type_horizontal_double_arrow;
    jint type_no_drop;
    jint type_null;
    jint type_text;
    jint type_top_left_diagonal_double_arrow;
    jint type_top_right_diagonal_double_arrow;
    jint type_vertical_double_arrow;
    jint type_vertical_text;
    jint type_wait;
    jint type_zoom_in;
    jint type_zoom_out;
    GHashTable *gdk_type_mapping;
  } a_pointericon;
  struct
  {
    jclass klass;
    jmethodID create_from_array;
    jobject argb8888;
  } a_bitmap;
  struct
  {
    jclass klass;
    jmethodID constructor;
    jmethodID constructor_action;
    jmethodID create_chooser;
    jmethodID get_data;
    jmethodID get_clipdata;
    jmethodID set_data_norm;
    jmethodID add_flags;
    jmethodID put_extra_bool;
    jmethodID put_extra_int;
    jmethodID put_extra_int_array;
    jmethodID put_extra_long;
    jmethodID put_extra_string;
    jmethodID put_extra_string_array;
    jmethodID put_extra_parcelable;
    jmethodID put_extras_from_bundle;
    jmethodID set_type;
    jmethodID normalize_mimetype;
    jint flag_activity_clear_task;
    jint flag_activity_multiple_task;
    jint flag_activity_new_task;
    jint flag_activity_no_animation;
    jint flag_grant_read_perm;
    jint flag_grant_write_perm;
    jstring action_create_document;
    jstring action_open_document;
    jstring action_open_document_tree;
    jstring action_edit;
    jstring action_view;
    jstring category_openable;
    jstring extra_allow_multiple;
    jstring extra_mimetypes;
    jstring extra_title;
    jstring extra_customtabs_session;
    jstring extra_customtabs_toolbar_color;
  } a_intent;
  struct {
    jclass klass;
    jmethodID constructor;
    jmethodID put_binder;
  } a_bundle;
  struct
  {
    jclass klass;
    jmethodID get_surface;
    jmethodID get_surface_frame;
    jmethodID lock_canvas;
    jmethodID lock_canvas_dirty;
    jmethodID unlock_canvas_and_post;
  } a_surfaceholder;
  struct
  {
    jclass klass;
    jmethodID draw_color;
  } a_canvas;
  struct
  {
    jclass klass;
    jobject clear;
  } a_blendmode;
  struct
  {
    jclass klass;
    jmethodID constructor;
    jfieldID bottom;
    jfieldID left;
    jfieldID right;
    jfieldID top;
  } a_rect;
  struct
  {
    jclass klass;
    jmethodID constructor;
  } a_rectf;
  struct
  {
    jclass klass;
    jmethodID get_device;
  } a_input_event;
  struct
  {
    jclass klass;
    jmethodID get_device_from_id;
    jmethodID get_motion_range;
  } a_input_device;
  struct
  {
    jclass klass;
    jmethodID get_axis;
    jmethodID get_min;
    jmethodID get_max;
    jmethodID get_resolution;
  } a_motion_range;
  struct
  {
    jclass klass;
    jmethodID get;
  } a_key_character_map;
  struct
  {
    jclass klass;
    jmethodID get_action;
    jmethodID get_clip_data;
    jmethodID get_clip_description;
    jmethodID get_local_state;
    jmethodID get_result;
    jmethodID get_x;
    jmethodID get_y;
    jint action_started;
    jint action_entered;
    jint action_location;
    jint action_exited;
    jint action_ended;
    jint action_drop;
  } a_drag_event;
  struct
  {
    jclass klass;
    jmethodID move_task_to_font;
  } a_activity_manager;
  struct
  {
    jclass klass;
    jmethodID get_path;
    jmethodID get_scheme;
    jmethodID normalize;
    jmethodID parse;
  } a_uri;
  struct
  {
    jclass klass;
    jmethodID get_channel;
  } j_file_istream;
  struct {
    jclass klass;
    jmethodID close;
    jmethodID read;
    jmethodID skip;
  } j_istream;
  struct
  {
    jclass klass;
    jmethodID get_channel;
  } j_file_ostream;
  struct
  {
    jclass klass;
    jmethodID close;
    jmethodID flush;
    jmethodID write;
  } j_ostream;
  struct {
    jclass klass;
    jmethodID get_position;
    jmethodID set_position;
    jmethodID get_size;
    jmethodID truncate;
  } j_file_channel;
  struct
  {
    jclass klass;
    jmethodID guess_content_type_for_name;
    jstring mime_binary_data;
  } j_urlconnection;
  struct
  {
    jclass klass;
    jmethodID constructor;
  } j_arraylist;
  struct
  {
    jclass klass;
    jmethodID add;
    jmethodID get;
    jmethodID size;
    jmethodID to_array;
  } j_list;
  struct
  {
    jclass klass;
  } j_string;
  struct
  {
    jclass klass;
    jmethodID equals;
    jmethodID hash_code;
    jmethodID to_string;
  } j_object;
  struct
  {
    jclass klass;
  } j_char_conversion_exception;
  struct
  {
    jclass io_exception;
    jclass eof_exception;
    jclass not_found_exception;
    jclass access_denied_exception;
    jclass not_empty_exception;
    jclass exists_exception;
    jclass loop_exception;
    jclass no_file_exception;
    jclass not_dir_exception;
    jclass malformed_uri_exception;
    jclass channel_closed_exception;
  } j_exceptions;
  struct
  {
    jclass klass;
    jmethodID get_message;
  } j_throwable;
} GdkAndroidJavaCache;

typedef struct
{
  JNIEnv *env;
  gboolean needs_detach;
} GdkAndroidThreadGuard;

JNIEnv *gdk_android_get_env (void);

GdkAndroidThreadGuard gdk_android_get_thread_env (void);
void gdk_android_drop_thread_env (GdkAndroidThreadGuard *self);

jobject gdk_android_get_activity        (void);
void    gdk_android_set_latest_activity (JNIEnv *env, jobject activity);

jobject gdk_android_init_get_user_classloader (void);
jclass gdk_android_init_find_class_using_classloader (JNIEnv *env, jobject class_loader, const gchar *klass);

const GdkAndroidJavaCache *gdk_android_get_java_cache (void);

G_END_DECLS
