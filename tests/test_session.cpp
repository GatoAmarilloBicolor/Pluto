// Integration test for SessionStore: roundtrip save/load/list/tag/remove
// against a tmpdir. Uses set_dir_override so it doesn't touch the user's
// real ~/config/settings/pluto/sessions/ directory.

#include "../src/storage/SessionStore.h"
#include "../src/provider/Provider.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

using pluto::Message;
using pluto::ContentBlock;
using pluto::SessionStore;
using pluto::SessionMetadata;

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-sess-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) {
		std::fprintf(stderr, "mkdtemp failed\n");
		std::exit(2);
	}
	return std::string(d);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	std::system(cmd.c_str());
}

static Message make_user(const std::string& text) {
	Message m;
	m.role = "user";
	ContentBlock b;
	b.type = ContentBlock::TEXT;
	b.text = text;
	m.content.push_back(std::move(b));
	return m;
}

static Message make_assistant(const std::string& text) {
	Message m;
	m.role = "assistant";
	ContentBlock b;
	b.type = ContentBlock::TEXT;
	b.text = text;
	m.content.push_back(std::move(b));
	return m;
}

static void test_save_and_load() {
	std::vector<Message> msgs;
	msgs.push_back(make_user("hello pluto"));
	msgs.push_back(make_assistant("hi! how can i help?"));
	msgs.push_back(make_user("write me a haiku"));

	std::string err;
	std::string path = SessionStore::save(
		"", "first session", "claude-test-model",
		123, 456, msgs, err);
	CHECK(!path.empty());
	CHECK(err.empty());

	std::vector<Message> loaded;
	SessionMetadata meta;
	bool ok = SessionStore::load(path, loaded, meta, err);
	CHECK(ok);
	CHECK(err.empty());
	CHECK(loaded.size() == 3);
	CHECK(loaded[0].role == "user");
	CHECK(loaded[0].content.size() == 1);
	CHECK(loaded[0].content[0].text == "hello pluto");
	CHECK(loaded[2].content[0].text == "write me a haiku");

	CHECK(meta.title == "first session");
	CHECK(meta.model == "claude-test-model");
	CHECK(meta.tokens_in == 123);
	CHECK(meta.tokens_out == 456);
	CHECK(meta.turns == 3);
}

static void test_list_and_recent() {
	auto all = SessionStore::list();
	CHECK(all.size() >= 1);

	auto recent = SessionStore::most_recent();
	CHECK(!recent.path.empty());
}

static void test_find_and_tags() {
	auto match = SessionStore::find_by_title("first");
	CHECK(!match.path.empty());
	CHECK(match.title == "first session");

	std::string err;
	bool ok = SessionStore::add_tag(match.path, "demo", err);
	CHECK(ok);
	ok = SessionStore::add_tag(match.path, "test", err);
	CHECK(ok);

	auto tags = SessionStore::get_tags(match.path);
	CHECK(tags.size() == 2);

	ok = SessionStore::remove_tag(match.path, "demo", err);
	CHECK(ok);
	tags = SessionStore::get_tags(match.path);
	CHECK(tags.size() == 1);
	CHECK(tags[0] == "test");
}

static void test_remove() {
	auto match = SessionStore::find_by_title("first");
	CHECK(!match.path.empty());

	std::string err;
	bool ok = SessionStore::remove(match.path, err);
	CHECK(ok);

	auto after = SessionStore::find_by_title("first");
	CHECK(after.path.empty());
}

int main() {
	std::string tmp = make_tmpdir();
	SessionStore::set_dir_override(tmp);

	test_save_and_load();
	test_list_and_recent();
	test_find_and_tags();
	test_remove();

	rm_rf(tmp);

	if (g_failures == 0) {
		std::fprintf(stderr, "test_session: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_session: %d failure(s)\n", g_failures);
	return 1;
}
