#include "clar_libgit2.h"

static git_repository *_repo;
static int counter;

void test_online_fetch__initialize(void)
{
	cl_git_pass(git_repository_init(&_repo, "./fetch", 0));
}

void test_online_fetch__cleanup(void)
{
	git_repository_free(_repo);
	_repo = NULL;

	cl_fixture_cleanup("./fetch");
}

static int update_tips(const char *refname, const git_oid *a, const git_oid *b, void *data)
{
	GIT_UNUSED(refname); GIT_UNUSED(a); GIT_UNUSED(b); GIT_UNUSED(data);

	++counter;

	return 0;
}

static int progress(const git_transfer_progress *stats, void *payload)
{
	size_t *bytes_received = (size_t *)payload;
	*bytes_received = stats->received_bytes;
	return 0;
}

static void do_fetch(const char *url, git_remote_autotag_option_t flag, int n)
{
	git_remote *remote;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	size_t bytes_received = 0;

	callbacks.update_tips = update_tips;
	counter = 0;

	cl_git_pass(git_remote_create(&remote, _repo, "test", url));
	git_remote_set_callbacks(remote, &callbacks);
	git_remote_set_autotag(remote, flag);
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(remote, progress, &bytes_received));
	cl_git_pass(git_remote_update_tips(remote));
	git_remote_disconnect(remote);
	cl_assert_equal_i(counter, n);
	cl_assert(bytes_received > 0);

	git_remote_free(remote);
}

void test_online_fetch__default_git(void)
{
	do_fetch("git://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_online_fetch__default_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_online_fetch__no_tags_git(void)
{
	do_fetch("git://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}

void test_online_fetch__no_tags_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}

static int transferProgressCallback(const git_transfer_progress *stats, void *payload)
{
	bool *invoked = (bool *)payload;

	GIT_UNUSED(stats);
	*invoked = true;
	return 0;
}

void test_online_fetch__doesnt_retrieve_a_pack_when_the_repository_is_up_to_date(void)
{
	git_repository *_repository;
	bool invoked = false;
	git_remote *remote;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.bare = true;

	cl_git_pass(git_clone(&_repository, "https://github.com/libgit2/TestGitRepository.git",
				"./fetch/lg2", &opts));
	git_repository_free(_repository);

	cl_git_pass(git_repository_open(&_repository, "./fetch/lg2"));

	cl_git_pass(git_remote_load(&remote, _repository, "origin"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));

	cl_assert_equal_i(false, invoked);

	cl_git_pass(git_remote_download(remote, &transferProgressCallback, &invoked));

	cl_assert_equal_i(false, invoked);

	cl_git_pass(git_remote_update_tips(remote));
	git_remote_disconnect(remote);

	git_remote_free(remote);
	git_repository_free(_repository);
}

static int cancel_at_half(const git_transfer_progress *stats, void *payload)
{
	GIT_UNUSED(payload);

	if (stats->received_objects > (stats->total_objects/2))
		return -1;
	return 0;
}

void test_online_fetch__can_cancel(void)
{
	git_remote *remote;
	size_t bytes_received = 0;

	cl_git_pass(git_remote_create(&remote, _repo, "test",
				"http://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_fail_with(git_remote_download(remote, cancel_at_half, &bytes_received), GIT_EUSER);
	git_remote_disconnect(remote);
	git_remote_free(remote);
}

int ls_cb(git_remote_head *rhead, void *payload)
{
	int *nr = payload;
	GIT_UNUSED(rhead);

	(*nr)++;

	return 0;
}

void test_online_fetch__ls_disconnected(void)
{
	git_remote *remote;
	int nr_before = 0, nr_after = 0;

	cl_git_pass(git_remote_create(&remote, _repo, "test",
				"http://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_ls(remote, ls_cb, &nr_before));
	git_remote_disconnect(remote);
	cl_git_pass(git_remote_ls(remote, ls_cb, &nr_after));

	cl_assert_equal_i(nr_before, nr_after);

	git_remote_free(remote);
}
