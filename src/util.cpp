// Common utility functions and RAII helpers.

#include "util.h"

#include <cctype>
#include <cstdlib>

#include <sys/stat.h>

namespace pluto {


std::string
HomeDir()
{
	const char* h = std::getenv("HOME");
	if (h != NULL) return h;
	return "/boot/home";
}

std::string
SettingsDir()
{
	return HomeDir() + "/config/settings/pluto";
}

std::string
EnsureSettingsDir()
{
	std::string dir = SettingsDir();
	::mkdir(dir.c_str(), 0755);
	return dir;
}

std::string
Slugify(const std::string& text)
{
	std::string slug;
	for (char c : text) {
		if (std::isalnum(static_cast<unsigned char>(c)))
			slug += std::tolower(static_cast<unsigned char>(c));
		else if ((c == ' ' || c == '-' || c == '_') && !slug.empty() && slug.back() != '-')
			slug += '-';
	}
	while (!slug.empty() && slug.back() == '-') slug.pop_back();
	if (slug.empty()) slug = "entry";
	return slug;
}

std::string
Trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;

	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;

	return s.substr(start, end - start);
}

PipeGuard::PipeGuard(const char* command, const char* mode)
    : fPipe(popen(command, mode))
{
}

PipeGuard::~PipeGuard()
{
	close();
}

int
PipeGuard::close()
{
	if (fPipe == NULL) return -1;
	int rc = pclose(fPipe);
	fPipe = NULL;
	return rc;
}


} // namespace pluto
