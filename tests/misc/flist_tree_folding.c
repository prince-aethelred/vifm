#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/sort.h"

#include "utils.h"

static void column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info);
static void toggle_fold_and_update(view_t *view);

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
	conf_setup();
	update_string(&cfg.fuse_home, "no");

	view_setup(&lwin);
	lwin.columns = columns_create();

	columns_set_line_print_func(&column_line_print);
}

TEARDOWN()
{
	conf_teardown();
	view_teardown(&lwin);

	columns_set_line_print_func(NULL);
}

TEST(no_folding_in_non_cv)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(11, lwin.list_rows);

	flist_toggle_fold(&lwin);
	assert_int_equal(11, lwin.list_rows);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(11, lwin.list_rows);
}

TEST(no_folding_for_non_dirs)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/file4", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);

	lwin.list_pos = 2;
	assert_string_equal("file4", lwin.dir_entry[lwin.list_pos].name);

	flist_toggle_fold(&lwin);
	assert_int_equal(3, lwin.list_rows);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(3, lwin.list_rows);
}

TEST(folding_of_directories)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	toggle_fold_and_update(&lwin);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("nested-dir", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	toggle_fold_and_update(&lwin);

	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("nested-dir", lwin.dir_entry[0].name);
	assert_string_equal("a", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[0].folded);

	assert_success(remove(SANDBOX_PATH "/nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(folding_two_tree_out_of_cv)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4/file3",
			cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("dir4", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	toggle_fold_and_update(&lwin);
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("dir4", lwin.dir_entry[0].name);
	assert_string_equal("file3", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[0].folded);
}

TEST(unfolding_accounts_for_sorting)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file1",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file2",
			cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_string_equal("file1", lwin.dir_entry[1].name);
	assert_string_equal("file2", lwin.dir_entry[2].name);

	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	lwin.sort[0] = -SK_BY_NAME;
	sort_view(&lwin);

	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_string_equal("file2", lwin.dir_entry[1].name);
	assert_string_equal("file1", lwin.dir_entry[2].name);
	assert_false(lwin.dir_entry[0].folded);
}

TEST(folding_five_tree_out_of_cv)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file1",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file2",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4/file3",
			cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(6, lwin.list_rows);

	assert_string_equal("dir2", lwin.dir_entry[0].name);
	assert_string_equal("dir3", lwin.dir_entry[1].name);
	assert_string_equal("file1", lwin.dir_entry[2].name);
	assert_string_equal("file2", lwin.dir_entry[3].name);
	assert_string_equal("dir4", lwin.dir_entry[4].name);
	assert_string_equal("file3", lwin.dir_entry[5].name);

	lwin.list_pos = 1;
	toggle_fold_and_update(&lwin);
	assert_int_equal(4, lwin.list_rows);

	toggle_fold_and_update(&lwin);
	assert_int_equal(6, lwin.list_rows);
}

TEST(folds_of_custom_tree_are_not_lost_on_filtering)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file1",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file2",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4/file3",
			cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(6, lwin.list_rows);

	/* fold */
	lwin.list_pos = 1;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 2;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);

	/* filter */
	assert_int_equal(0, local_filter_set(&lwin, "[34]"));
	local_filter_accept(&lwin);
	assert_int_equal(2, lwin.list_rows);

	/* unfold */
	lwin.list_pos = 0;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	/* remove filter */
	local_filter_remove(&lwin);
	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(&lwin));
	curr_stats.load_stage = 0;
	assert_int_equal(5, lwin.list_rows);
}

/* This test mixes different trees and does reloading to verify resource uses
 * and tree reloading. */
TEST(folding_grind)
{
	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/file4", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file1",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir3/file2",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir1/dir2/dir4/file3",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir5", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir5/.nested_hidden",
			cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/dir5/file5", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "tree/.hidden", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(CV_CUSTOM_TREE, lwin.custom.type);
	assert_int_equal(13, lwin.list_rows);

	lwin.list_pos = 3;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 4;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 2;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 4;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(6, lwin.list_rows);

	lwin.list_pos = 2;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(8, lwin.list_rows);

	/* Not a custom tree below. */

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(CV_TREE, lwin.custom.type);
	assert_int_equal(12, lwin.list_rows);

	lwin.list_pos = 2;
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 3;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 3;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(7, lwin.list_rows);
}

static void
column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

static void
toggle_fold_and_update(view_t *view)
{
	flist_toggle_fold(view);

	validate_tree(&lwin);

	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(view));
	curr_stats.load_stage = 0;

	validate_tree(&lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */