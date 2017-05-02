/* Scrolling test suite for GtkTreeView
 * Copyright (C) 2006  Kristian Rietveld  <kris@gtk.org>
 * Copyright (C) 2007  Imendio AB,  Kristian Rietveld
 * Copyright (C) 2009  Kristian Rietveld  <kris@gtk.org>
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

/* Original v1.0 -- December 26, 2006
 * Conversion to GLib/GTK+ test framework during December, 2007
 */


#include <gtk/gtk.h>
#include <unistd.h>

#define VIEW_WIDTH 320
#define VIEW_HEIGHT 240

#define N_ROWS 1000
#define BIG_N_ROWS N_ROWS * 100

/*
 * To do:
 *   - Test that nothing happens if the row is fully visible.
 *   - The tests are dependent on the theme/font (size measurements,
 *     chosen paths).
 *   - Convert to proper GTK+ coding style.
 *   - Briefly test scrolling in tree stores as well.
 */


/* Constructing models for testing */
static GtkTreeModel *
create_model (gboolean constant)
{
	int i;

	GtkTreeIter iter;
	GtkListStore *store;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (i = 0; i < N_ROWS; i++) {
		gtk_list_store_append (store, &iter);
		if (constant || i % 2 == 0)
			gtk_list_store_set (store, &iter, 0, "Foo", -1);
		else
			gtk_list_store_set (store, &iter, 0, "Sliff\nSloff\nBleh", -1);
	}

	return GTK_TREE_MODEL (store);
}

static GtkTreeModel *
create_big_model (gboolean constant)
{
	int i;

	GtkTreeIter iter;
	GtkListStore *store;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (i = 0; i < BIG_N_ROWS; i++) {
		gtk_list_store_append (store, &iter);
		if (constant || i % 2 == 0)
			gtk_list_store_set (store, &iter, 0, "Foo", -1);
		else
			gtk_list_store_set (store, &iter, 0, "Sliff\nSloff\nBleh", -1);
	}

	return GTK_TREE_MODEL (store);
}

/*
 * Fixtures
 */

typedef struct
{
	GtkWidget *window;
	GtkWidget *tree_view;
}
ScrollFixture;

static void
scroll_fixture_setup (ScrollFixture *fixture,
		      GtkTreeModel  *model,
		      gconstpointer  test_data)
{
	GtkWidget *sw;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (fixture->window), sw);

	fixture->tree_view = gtk_tree_view_new_with_model (model);
	g_object_unref (model);
	gtk_widget_set_size_request (fixture->tree_view, VIEW_WIDTH, VIEW_HEIGHT);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes ("Title",
							   renderer,
							   "text", 0,
							   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (fixture->tree_view), column);
	gtk_container_add (GTK_CONTAINER (sw), fixture->tree_view);
}

/* sets up a fixture with a model with constant row heights */
static void
scroll_fixture_constant_setup (ScrollFixture *fixture,
			       gconstpointer  test_data)
{
	scroll_fixture_setup (fixture, create_model (TRUE), test_data);
}

/* sets up a fixture with a model with varying row heights */
static void
scroll_fixture_mixed_setup (ScrollFixture *fixture,
			    gconstpointer  test_data)
{
	scroll_fixture_setup (fixture, create_model (FALSE), test_data);
}

/* sets up a fixture with a large model with constant row heights */
static void
scroll_fixture_constant_big_setup (ScrollFixture *fixture,
			           gconstpointer  test_data)
{
	scroll_fixture_setup (fixture, create_big_model (TRUE), test_data);
}

/* sets up a fixture with a large model with varying row heights */
static void
scroll_fixture_mixed_big_setup (ScrollFixture *fixture,
			        gconstpointer  test_data)
{
	scroll_fixture_setup (fixture, create_big_model (FALSE), test_data);
}

/* sets up a fixture with only a single row for the "single row scroll" test */
static void
scroll_fixture_single_setup (ScrollFixture *fixture,
			     gconstpointer  test_data)
{
	GtkTreeStore *store;
	GtkTreeIter iter, child;

	store = gtk_tree_store_new (1, G_TYPE_STRING);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, "Foo", -1);

	gtk_tree_store_append (store, &child, &iter);
	gtk_tree_store_set (store, &child, 0, "Two\nLines", -1);

	/* The teardown will also destroy the model */
	scroll_fixture_setup (fixture, GTK_TREE_MODEL (store), test_data);
}

/* sets up a fixture with a tree store */
static void
scroll_fixture_tree_setup (ScrollFixture *fixture,
			   gconstpointer   test_data)
{
	GtkTreeStore *store;
	GtkTreeIter iter, child;
	int i;

	store = gtk_tree_store_new (1, G_TYPE_STRING);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, "Root node", -1);

	for (i = 0; i < 5; i++) {
		gtk_tree_store_append (store, &child, &iter);
		gtk_tree_store_set (store, &child, 0, "Child node", -1);
	}

	for (i = 0; i < 5; i++) {
		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter, 0, "Other node", -1);
	}

	/* The teardown will also destroy the model */
	scroll_fixture_setup (fixture, GTK_TREE_MODEL (store), test_data);
}

static void
scroll_fixture_teardown (ScrollFixture *fixture,
			 gconstpointer  test_data)
{
	gtk_widget_destroy (fixture->window);
}

/*
 * Position check and helpers.
 */
enum Pos
{
	POS_TOP,
	POS_CENTER,
	POS_BOTTOM
};

static int
get_row_start_for_index (GtkTreeView *tree_view, int index)
{
	gint height1, height2;
	gint row_start;
	GtkTreePath *path;
	GdkRectangle rect;

	path = gtk_tree_path_new_from_indices (0, -1);
	gtk_tree_view_get_background_area (tree_view, path, NULL, &rect);
	height1 = rect.height;

	gtk_tree_path_next (path);
	gtk_tree_view_get_background_area (tree_view, path, NULL, &rect);
	height2 = rect.height;
	gtk_tree_path_free (path);

	row_start = (index / 2) * height1 + (index / 2) * height2;
	if (index % 2)
		row_start += height1;

	return row_start;
}

static enum Pos
get_pos_from_path (GtkTreeView   *tree_view,
		   GtkTreePath   *path,
		   gint           row_height,
		   GtkAdjustment *vadj)
{
	int row_start;

	row_start = get_row_start_for_index (tree_view,
					     gtk_tree_path_get_indices (path)[0]);

	if (row_start + row_height < vadj->page_size)
		return POS_TOP;

	if (row_start >= vadj->upper - vadj->page_size)
		return POS_BOTTOM;

	return POS_CENTER;
}

static gboolean
test_position_with_align (GtkTreeView  *tree_view,
			  enum Pos      pos,
			  gint          row_y,
			  gint          row_start,
			  gint          row_height,
			  gfloat        row_align)
{
	gboolean passed = TRUE;
	GtkAdjustment *vadj = gtk_tree_view_get_vadjustment (tree_view);

	/* Switch on row-align: 0.0, 0.5, 1.0 */
	switch ((int)(row_align * 2.)) {
	case 0:
		if (pos == POS_TOP || pos == POS_CENTER) {
			/* The row in question is the first row
			 * in the view.
			 *    - rect.y should be zero
			 *    - dy should be equal to the top
			 *      y coordinate of the row.
			 */
			if (row_y != 0)
				passed = FALSE;
			if (vadj->value != row_start)
				passed = FALSE;
		} else {
			/* The row can be anywhere at the last
			 * page of the tree view.
			 *   - dy is set to the start of the
			 *     last page.
			 */
			if (vadj->value != vadj->upper - vadj->page_size)
				passed = FALSE;
		}
		break;

	case 1:
		/* 0.5 */
		if (pos == POS_TOP
		    && row_start < vadj->page_size / 2) {
			/* For the first half of the top view we can't
			 * center the row in the view, instead we
			 * show the first page.
			 *   - dy should be zero
			 */
			if (vadj->value != 0)
				passed = FALSE;
		} else if (pos == POS_BOTTOM
			   && row_start >= vadj->upper - vadj->page_size / 2) {
			/* For the last half of the bottom view we
			 * can't center the row in the view, instead
			 * we show the last page.
			 *   - dy should be the start of the 
			 *     last page.
			 */
			if (vadj->value != vadj->upper - vadj->page_size)
				passed = FALSE;
		} else {
			/* The row is located in the middle of
			 * the view.
			 *    - top y coordinate is equal to
			 *      middle of the view minus
			 *      half the height of the row.
			 *      (ie. the row's center is at the
			 *       center of the view).
			 */
			if (row_y != (int)(vadj->page_size / 2 - row_height / 2))
				passed = FALSE;
		}
		break;

	case 2:
		/* 1.0 */
		if (pos == POS_TOP) {
			/* The row can be anywhere on the
			 * first page of the tree view.
			 *   - dy is zero.
			 */
			if (vadj->value != 0)
				passed = FALSE;
		} else if (pos == POS_CENTER || pos == POS_BOTTOM) {
			/* The row is the last row visible in the
			 * view.
			 *   - rect.y is set to the top of the
			 *     last row.
			 *   - row_start is greater than page_size
			 *     (ie we are not on the first page).
			 *   - dy is greater than zero
			 */
			if (row_start < vadj->page_size
			    && row_start + row_height < vadj->page_size)
				passed = FALSE;
			if (vadj->value <= 0)
				passed = FALSE;
			if (row_y != vadj->page_size - row_height)
				passed = FALSE;
		}
		break;
	}

	return passed;
}

static gboolean
test_position_without_align (GtkTreeView *tree_view,
			     gint         row_start,
			     gint         row_height)
{
	GtkAdjustment *vadj = gtk_tree_view_get_vadjustment (tree_view);

	/* Without align the tree view does as less work as possible,
	 * so basically we only have to check whether the row
	 * is visible on the screen.
	 */
	if (vadj->value <= row_start
	    && vadj->value + vadj->page_size >= row_start + row_height)
		return TRUE;

	return FALSE;
}

static void
test_position (GtkTreeView *tree_view,
	       GtkTreePath *path,
	       gboolean     use_align,
	       gfloat       row_align,
	       gfloat       col_align)
{
	gint pos;
	gchar *path_str;
	GdkRectangle rect;
	GtkTreeModel *model;
	gint row_start;

	/* Get the location of the path we scrolled to */
	gtk_tree_view_get_background_area (GTK_TREE_VIEW (tree_view),
					   path, NULL, &rect);

	row_start = get_row_start_for_index (GTK_TREE_VIEW (tree_view),
					     gtk_tree_path_get_indices (path)[0]);

	/* Ugh */
	pos = get_pos_from_path (GTK_TREE_VIEW (tree_view),
				 path, rect.height,
			         gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (tree_view)));

	/* This is only tested for during test_single() */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	if (gtk_tree_model_iter_n_children (model, NULL) == 1) {
		GtkTreePath *tmppath;

		/* Test nothing is dangling at the bottom; read
		 * description for test_single() for more information.
		 */

		/* FIXME: hardcoded width */
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), 0, GTK_WIDGET (tree_view)->allocation.height - 30, &tmppath, NULL, NULL, NULL)) {
			g_assert_not_reached ();
			gtk_tree_path_free (tmppath);
		}
	}

	path_str = gtk_tree_path_to_string (path);
	if (use_align) {
		g_assert (test_position_with_align (tree_view, pos, rect.y,
						    row_start, rect.height, row_align));
	} else {
		g_assert (test_position_without_align (tree_view, row_start, rect.height));
	}

	g_free (path_str);
}

/*
 * Scrolling code
 */


/* Testing scrolling to various positions with various alignments */

static void
scroll (ScrollFixture *fixture,
	GtkTreePath   *path,
	gboolean       use_align,
	gfloat         row_align)
{
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (fixture->tree_view), path,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fixture->tree_view),
				      path, NULL,
				      use_align, row_align, 0.0);

	gtk_widget_show_all (fixture->window);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	test_position (GTK_TREE_VIEW (fixture->tree_view), path,
		       use_align, row_align, 0.0);
}

static void
scroll_no_align (ScrollFixture *fixture,
		 gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll (fixture, path, FALSE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_align_0_0 (ScrollFixture *fixture,
		  gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll (fixture, path, TRUE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_align_0_5 (ScrollFixture *fixture,
		  gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll (fixture, path, TRUE, 0.5);
	gtk_tree_path_free (path);
}

static void
scroll_align_1_0 (ScrollFixture *fixture,
		  gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll (fixture, path, TRUE, 1.0);
	gtk_tree_path_free (path);
}


static void
scroll_after_realize (ScrollFixture *fixture,
		      GtkTreePath   *path,
		      gboolean       use_align,
		      gfloat         row_align)
{
	gtk_widget_show_all (fixture->window);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (fixture->tree_view), path,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fixture->tree_view),
				      path, NULL,
				      use_align, row_align, 0.0);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	test_position (GTK_TREE_VIEW (fixture->tree_view), path,
		       use_align, row_align, 0.0);
}

static void
scroll_after_no_align (ScrollFixture *fixture,
		       gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_after_realize (fixture, path, FALSE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_after_align_0_0 (ScrollFixture *fixture,
		        gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_after_realize (fixture, path, TRUE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_after_align_0_5 (ScrollFixture *fixture,
		        gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_after_realize (fixture, path, TRUE, 0.5);
	gtk_tree_path_free (path);
}

static void
scroll_after_align_1_0 (ScrollFixture *fixture,
		        gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_after_realize (fixture, path, TRUE, 1.0);
	gtk_tree_path_free (path);
}


static void
scroll_both_realize (ScrollFixture *fixture,
		     GtkTreePath   *path,
		     gboolean       use_align,
		     gfloat         row_align)
{
	GtkTreePath *end;

	gtk_widget_show_all (fixture->window);

	/* Scroll to end */
	end = gtk_tree_path_new_from_indices (999, -1);

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (fixture->tree_view), end,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fixture->tree_view),
				      end, NULL,
				      use_align, row_align, 0.0);
	gtk_tree_path_free (end);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* Scroll to final position */
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (fixture->tree_view), path,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fixture->tree_view),
				      path, NULL,
				      use_align, row_align, 0.0);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	test_position (GTK_TREE_VIEW (fixture->tree_view), path,
		       use_align, row_align, 0.0);
}

static void
scroll_both_no_align (ScrollFixture *fixture,
		      gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_both_realize (fixture, path, FALSE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_both_align_0_0 (ScrollFixture *fixture,
		       gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_both_realize (fixture, path, TRUE, 0.0);
	gtk_tree_path_free (path);
}

static void
scroll_both_align_0_5 (ScrollFixture *fixture,
		       gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_both_realize (fixture, path, TRUE, 0.5);
	gtk_tree_path_free (path);
}

static void
scroll_both_align_1_0 (ScrollFixture *fixture,
		       gconstpointer  test_data)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (test_data);
	scroll_both_realize (fixture, path, TRUE, 1.0);
	gtk_tree_path_free (path);
}

/* Testing scrolling to a newly created row */
static void
create_new_row (GtkListStore *store,
		int           n,
		GtkTreeIter  *iter)
{
	switch (n) {
		case 0:
			/* Prepend a row */
			gtk_list_store_prepend (store, iter);
			break;

		case 4:
			/* Add a row in the middle of the visible area */
			gtk_list_store_insert (store, iter, 4);
			break;

		case 8:
			/* Add a row which is not completely visible */
			gtk_list_store_insert (store, iter, 8);
			break;

		case 500:
			/* Add a row in the middle */
			gtk_list_store_insert (store, iter, 500);
			break;

		case 999:
			/* Append a row */
			gtk_list_store_append (store, iter);
			break;
	}

	gtk_list_store_set (store, iter, 0, "New...", -1);
}

static void
scroll_new_row_editing_started (GtkCellRenderer *cell,
				GtkCellEditable *editable,
				const char      *path,
				gpointer         user_data)
{
	GtkWidget **widget = user_data;

	*widget = GTK_WIDGET (editable);
}

static void
test_editable_position (GtkWidget   *tree_view,
			GtkWidget   *editable,
			GtkTreePath *cursor_path)
{
	GdkRectangle rect;
	GtkAdjustment *vadj;

	gtk_tree_view_get_background_area (GTK_TREE_VIEW (tree_view),
					   cursor_path, NULL, &rect);

	vadj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (tree_view));

	/* There are all in bin_window coordinates */
	g_assert (editable->allocation.y == rect.y + ((rect.height - editable->allocation.height) / 2));
}

static void
scroll_new_row (ScrollFixture *fixture,
		gconstpointer  test_data)
{
	GtkTreeIter scroll_iter;
	GtkTreePath *scroll_path;
	GtkTreeModel *model;
	GList *renderers;
	GtkTreeViewColumn *column;
	GtkWidget *editable;

	/* The aim of this test is creating a new row at several places,
	 * and immediately put the cursor on it.  TreeView should correctly
	 * scroll to the row and show the editable widget.
	 *
	 * See #81627.
	 */

	g_test_bug ("81627");

	gtk_widget_show_all (fixture->window);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* Create the new row and scroll to it */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (fixture->tree_view));
	create_new_row (GTK_LIST_STORE (model), GPOINTER_TO_INT (test_data),
			&scroll_iter);

	/* Set up a signal handler to acquire the editable widget */
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (fixture->tree_view), 0);
	renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));

	g_signal_connect (G_OBJECT (renderers->data), "editing-started",
			  G_CALLBACK (scroll_new_row_editing_started),
			  &editable);

	/* Now set the cursor on the path and start editing */
	scroll_path = gtk_tree_model_get_path (model, &scroll_iter);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (fixture->tree_view),
				  scroll_path,
				  column,
				  TRUE);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* Test position */
	test_position (GTK_TREE_VIEW (fixture->tree_view), scroll_path,
		       FALSE, 0.0, 0.0);
	test_editable_position (fixture->tree_view, editable, scroll_path);

	gtk_tree_path_free (scroll_path);
}

static void
scroll_new_row_tree (ScrollFixture *fixture,
		     gconstpointer  test_data)
{
	GtkTreeModel *model;
	GtkAdjustment *vadjustment;
	int i;

	/* The goal of this test is to append new rows at the end of a tree
	 * store and immediately scroll to them.  If there is a parent
	 * node with a couple of childs in the "area above" to explore,
	 * this used to lead to unexpected results due to a bug.
	 *
	 * This issue has been reported by Miroslav Rajcic on
	 * gtk-app-devel-list:
	 * http://mail.gnome.org/archives/gtk-app-devel-list/2008-December/msg00068.html
	 */

	gtk_widget_show_all (fixture->window);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (fixture->tree_view));

	while (gtk_events_pending ())
		gtk_main_iteration ();

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (fixture->tree_view));
	vadjustment = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (fixture->tree_view));

	for (i = 0; i < 5; i++) {
		GtkTreeIter scroll_iter;
		GtkTreePath *scroll_path;

		gtk_tree_store_append (GTK_TREE_STORE (model), &scroll_iter,
				       NULL);
		gtk_tree_store_set (GTK_TREE_STORE (model), &scroll_iter,
				    0, "New node", -1);

		scroll_path = gtk_tree_model_get_path (model, &scroll_iter);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fixture->tree_view),
					      scroll_path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free (scroll_path);

		while (gtk_events_pending ())
			gtk_main_iteration ();

		/* Test position, the scroll bar must be at the end */
		g_assert (vadjustment->value == vadjustment->upper - vadjustment->page_size);
	}
}

/* Test for GNOME bugzilla bug 359231; tests "recovery when removing a bunch of
 * rows at the bottom.
 */
static void
test_bug316689 (ScrollFixture *fixture,
		gconstpointer  test_data)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkAdjustment *vadj;
	GtkTreeModel *model;

	/* The aim of this test is to scroll to the bottom of a TreeView,
	 * remove at least one page_size of items and check if TreeView
	 * correctly corrects the scroll bar (else they will look "broken").
	 *
	 * See #316689.
	 */

	g_test_bug ("316689");

	/* Scroll to some place close to the end */
	path = gtk_tree_path_new_from_indices (N_ROWS - 4, -1);
	scroll (fixture, path, FALSE, 0.0);
	gtk_tree_path_free (path);

	/* No need for a while events pending loop here, scroll() does this for us.
	 *
	 * We now remove a bunch of rows, wait for events to process and then
	 * check the adjustments to see if the TreeView gracefully recovered.
	 */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (fixture->tree_view));

	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, N_ROWS - 15))
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	vadj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (fixture->tree_view));

	g_assert (vadj->value + vadj->page_size <= vadj->upper);
	g_assert (vadj->value == vadj->upper - vadj->page_size);
}


/* Test for GNOME bugzilla bug 359231 */
static void
test_bug359231 (void)
{
	int i;
	int height1, height2;
	GtkTreePath *path;
	GtkTreeIter iter, child;
	GtkTreeStore *store;
	GdkRectangle rect;
	ScrollFixture *fixture;

	/* See #359231. */
	g_test_bug ("359231");

	/* Create model (GtkTreeStore in this case) */
	store = gtk_tree_store_new (1, G_TYPE_STRING);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, "Foo", -1);

	for (i = 0; i < 4; i++) {
		gtk_tree_store_append (store, &child, &iter);
		gtk_tree_store_set (store, &child, 0, "Two\nLines", -1);
	}
	
	fixture = g_new0 (ScrollFixture, 1);
	scroll_fixture_setup (fixture, GTK_TREE_MODEL (store), NULL);
	gtk_widget_show_all (fixture->window);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* Prepend some rows at the top, expand */
	gtk_tree_store_prepend (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, "Foo", -1);

	gtk_tree_store_prepend (store, &child, &iter);
	gtk_tree_store_set (store, &child, 0, "Two\nLines", -1);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (fixture->tree_view));

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* Test if height of row 0:0 is correct */
	path = gtk_tree_path_new_from_indices (0, -1);
	gtk_tree_view_get_background_area (GTK_TREE_VIEW (fixture->tree_view),
					   path, NULL, &rect);
	height1 = rect.height;

	gtk_tree_path_down (path);
	gtk_tree_view_get_background_area (GTK_TREE_VIEW (fixture->tree_view),
					   path, NULL, &rect);
	height2 = rect.height;
	gtk_tree_path_free (path);

	g_assert (height2 > height1);

	/* Clean up; the tear down also cleans up the model */
	scroll_fixture_teardown (fixture, NULL);
}

/* Infrastructure for automatically adding tests */
enum
{
	BEFORE,
	AFTER,
	BOTH
};

static const char *
test_type_string (int test_type)
{
	switch (test_type) {
		case BEFORE:
			return "before-realize";

		case AFTER:
			return "after-realize";

		case BOTH:
			return "both";
	}

	return "???";
}

static char *
align_string (gboolean use_align,
	      gfloat   row_align)
{
	char *ret;

	if (!use_align)
		return g_strdup ("no-align");

	ret = g_strdup_printf ("align-%1.1f", row_align);
	return ret;
}

static void
add_test (const char *path,
	  gboolean    mixed,
	  int         test_type,
	  gboolean    use_align,
	  gfloat      row_align,
	  void (* setup) (ScrollFixture *, gconstpointer),
	  void (* scroll_func) (ScrollFixture *, gconstpointer))
{
	gchar *test_path;
	gchar *align;

	align = align_string (use_align, row_align);

	test_path = g_strdup_printf ("/TreeView/scrolling/%s/%s-height/path-%s-%s",
				     test_type_string (test_type),
				     mixed ? "mixed" : "constant",
				     path, align);
	g_free (align);

	g_test_add (test_path, ScrollFixture, path,
		    setup, scroll_func, scroll_fixture_teardown);

	g_free (test_path);
}

static void
add_tests (gboolean mixed,
	   int test_type,
	   gboolean use_align,
	   gfloat row_align,
	   void (*scroll_func) (ScrollFixture *, gconstpointer))
{
	void (* setup) (ScrollFixture *, gconstpointer);

	if (mixed)
		setup = scroll_fixture_mixed_setup;
	else
		setup = scroll_fixture_constant_setup;

	add_test ("0", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("2", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("5", mixed, test_type, use_align, row_align, setup, scroll_func);
	/* We scroll to 8 to test a partial visible row.  The 8 is
	 * based on my font setting of "Vera Sans 11" and
	 * the separators set to 0.  (This should be made dynamic; FIXME).
	 */
	add_test ("8", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("10", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("250", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("500", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("750", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("990", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("991", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("995", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("997", mixed, test_type, use_align, row_align, setup, scroll_func);
	add_test ("999", mixed, test_type, use_align, row_align, setup, scroll_func);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	/* Scrolls before realization */
	add_tests (FALSE, BEFORE, FALSE, 0.0, scroll_no_align);
	if (g_test_thorough ())
		add_tests (TRUE, BEFORE, FALSE, 0.0, scroll_no_align);

	add_tests (FALSE, BEFORE, TRUE, 0.0, scroll_align_0_0);
	if (g_test_thorough ())
		add_tests (TRUE, BEFORE, TRUE, 0.0, scroll_align_0_0);

	add_tests (FALSE, BEFORE, TRUE, 0.5, scroll_align_0_5);
	if (g_test_thorough ())
		add_tests (TRUE, BEFORE, TRUE, 0.5, scroll_align_0_5);

	add_tests (FALSE, BEFORE, TRUE, 1.0, scroll_align_1_0);
	if (g_test_thorough ())
		add_tests (TRUE, BEFORE, TRUE, 1.0, scroll_align_1_0);

	/* Scrolls after realization */
	add_tests (FALSE, AFTER, FALSE, 0.0, scroll_after_no_align);
	if (g_test_thorough ())
		add_tests (TRUE, AFTER, FALSE, 0.0, scroll_after_no_align);

	add_tests (FALSE, AFTER, TRUE, 0.0, scroll_after_align_0_0);
	if (g_test_thorough ())
		add_tests (TRUE, AFTER, TRUE, 0.0, scroll_after_align_0_0);

	add_tests (FALSE, AFTER, TRUE, 0.5, scroll_after_align_0_5);
	if (g_test_thorough ())
		add_tests (TRUE, AFTER, TRUE, 0.5, scroll_after_align_0_5);

	add_tests (FALSE, AFTER, TRUE, 1.0, scroll_after_align_1_0);
	if (g_test_thorough ())
		add_tests (TRUE, AFTER, TRUE, 1.0, scroll_after_align_1_0);

	/* Scroll to end before realization, to a real position after */
	if (g_test_thorough ()) {
		add_tests (FALSE, BOTH, FALSE, 0.0, scroll_both_no_align);
		add_tests (TRUE, BOTH, FALSE, 0.0, scroll_both_no_align);

		add_tests (FALSE, BOTH, TRUE, 0.0, scroll_both_align_0_0);
		add_tests (TRUE, BOTH, TRUE, 0.0, scroll_both_align_0_0);

		add_tests (FALSE, BOTH, TRUE, 0.5, scroll_both_align_0_5);
		add_tests (TRUE, BOTH, TRUE, 0.5, scroll_both_align_0_5);

		add_tests (FALSE, BOTH, TRUE, 1.0, scroll_both_align_1_0);
		add_tests (TRUE, BOTH, TRUE, 1.0, scroll_both_align_1_0);
	}

	/* Test different alignments in view with single row */
	g_test_add ("/TreeView/scrolling/single-row/no-align",
		    ScrollFixture, "0",
		    scroll_fixture_single_setup,
		    scroll_no_align,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/single-row/align-0.0",
		    ScrollFixture, "0",
		    scroll_fixture_single_setup,
		    scroll_align_0_0,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/single-row/align-0.5",
		    ScrollFixture, "0",
		    scroll_fixture_single_setup,
		    scroll_align_0_5,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/single-row/align-1.0",
		    ScrollFixture, "0",
		    scroll_fixture_single_setup,
		    scroll_align_1_0,
		    scroll_fixture_teardown);

	/* Test scrolling in a very large model; also very slow */
	if (g_test_slow ()) {
		g_test_add ("/TreeView/scrolling/large-model/constant-height/middle-no-align",
			    ScrollFixture, "50000",
			    scroll_fixture_constant_big_setup,
			    scroll_no_align,
			    scroll_fixture_teardown);
		g_test_add ("/TreeView/scrolling/large-model/constant-height/end-no-align",
			    ScrollFixture, "99999",
			    scroll_fixture_constant_big_setup,
			    scroll_no_align,
			    scroll_fixture_teardown);

		g_test_add ("/TreeView/scrolling/large-model/mixed-height/middle-no-align",
			    ScrollFixture, "50000",
			    scroll_fixture_mixed_big_setup,
			    scroll_no_align,
			    scroll_fixture_teardown);
		g_test_add ("/TreeView/scrolling/large-model/mixed-height/end-no-align",
			    ScrollFixture, "99999",
			    scroll_fixture_mixed_big_setup,
			    scroll_no_align,
			    scroll_fixture_teardown);
	}

	/* Test scrolling to a newly created row */
	g_test_add ("/TreeView/scrolling/new-row/path-0", ScrollFixture,
		    GINT_TO_POINTER (0),
		    scroll_fixture_constant_setup,
		    scroll_new_row,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/new-row/path-4", ScrollFixture,
		    GINT_TO_POINTER (4),
		    scroll_fixture_constant_setup,
		    scroll_new_row,
		    scroll_fixture_teardown);
	/* We scroll to 8 to test a partial visible row.  The 8 is
	 * based on my font setting of "Vera Sans 11" and
	 * the separators set to 0.  (This should be made dynamic; FIXME).
	 */
	g_test_add ("/TreeView/scrolling/new-row/path-8", ScrollFixture,
		    GINT_TO_POINTER (8),
		    scroll_fixture_constant_setup,
		    scroll_new_row,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/new-row/path-500", ScrollFixture,
		    GINT_TO_POINTER (500),
		    scroll_fixture_constant_setup,
		    scroll_new_row,
		    scroll_fixture_teardown);
	g_test_add ("/TreeView/scrolling/new-row/path-999", ScrollFixture,
		    GINT_TO_POINTER (999),
		    scroll_fixture_constant_setup,
		    scroll_new_row,
		    scroll_fixture_teardown);

	g_test_add ("/TreeView/scrolling/new-row/tree", ScrollFixture,
		    NULL,
		    scroll_fixture_tree_setup,
		    scroll_new_row_tree,
		    scroll_fixture_teardown);

	/* Misc. tests */
	g_test_add ("/TreeView/scrolling/specific/bug-316689",
			ScrollFixture, NULL,
		    scroll_fixture_constant_setup, test_bug316689,
		    scroll_fixture_teardown);
	g_test_add_func ("/TreeView/scrolling/specific/bug-359231",
			test_bug359231);

	return g_test_run ();
}
