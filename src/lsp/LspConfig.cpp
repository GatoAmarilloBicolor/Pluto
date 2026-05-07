/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "LspConfig.h"

#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>

#include <cstdlib>
#include <fstream>
#include <sstream>


namespace pluto {


namespace {


std::string
Trim(const std::string& text)
{
	size_t start = 0;
	size_t end = text.size();
	while (start < end && (text[start] == ' ' || text[start] == '\t'))
		start++;
	while (end > start
		&& (text[end - 1] == ' ' || text[end - 1] == '\t'
			|| text[end - 1] == '\r')) {
		end--;
	}
	return text.substr(start, end - start);
}


// Parse a single lsp.conf file. Format:
//
//   [clangd]
//   command = /boot/system/bin/clangd
//   args = --background-index --offset-encoding=utf-8
//   extensions = .cpp, .h, .cc, .c, .hpp
//
//   [pylsp]
//   command = pylsp
//   args = -v
//   extensions = .py
//
void
ParseLspConf(const std::string& path, std::vector<LspServerConfig>& configs)
{
	std::ifstream file(path);
	if (!file)
		return;

	LspServerConfig current;
	bool inSection = false;

	auto flushSection = [&]() {
		if (inSection && !current.command.empty()
			&& !current.extensions.empty()) {
			// Verify the command exists before adding.
			BEntry entry(current.command.c_str(), true);
			if (entry.Exists())
				configs.push_back(std::move(current));
		}
		current = LspServerConfig{};
		inSection = false;
	};

	std::string line;
	while (std::getline(file, line)) {
		std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == '#')
			continue;

		// Section header: [name]
		if (trimmed[0] == '[' && trimmed.back() == ']') {
			flushSection();
			current.name = trimmed.substr(1, trimmed.size() - 2);
			inSection = true;
			continue;
		}

		if (!inSection)
			continue;

		size_t equals = trimmed.find('=');
		if (equals == std::string::npos)
			continue;

		std::string key = Trim(trimmed.substr(0, equals));
		std::string value = Trim(trimmed.substr(equals + 1));

		if (key == "command") {
			current.command = value;
		} else if (key == "args") {
			// Split by whitespace.
			std::istringstream stream(value);
			std::string token;
			while (stream >> token)
				current.args.push_back(token);
		} else if (key == "extensions") {
			// Split by comma.
			std::istringstream stream(value);
			std::string token;
			while (std::getline(stream, token, ',')) {
				std::string ext = Trim(token);
				if (!ext.empty())
					current.extensions.push_back(ext);
			}
		}
	}

	flushSection();
}


}	// namespace


std::vector<LspServerConfig>
LoadLspConfigs()
{
	std::vector<LspServerConfig> configs;

	// 1. Built-in config shipped with the source.
	ParseLspConf("data/lsp.conf", configs);

	// 2. Packaged install path.
	if (configs.empty())
		ParseLspConf("/boot/system/data/pluto/lsp.conf", configs);

	// 3. User override — appends or replaces.
	BPath settingsPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath) == B_OK) {
		settingsPath.Append("pluto/lsp.conf");
		ParseLspConf(settingsPath.Path(), configs);
	}

	return configs;
}


const LspServerConfig*
FindLspServer(const std::vector<LspServerConfig>& configs,
	const std::string& extension)
{
	for (const auto& config : configs) {
		for (const auto& ext : config.extensions) {
			if (ext == extension)
				return &config;
		}
	}
	return NULL;
}


std::string
FileExtension(const std::string& path)
{
	// Find the last component (basename).
	size_t slash = path.rfind('/');
	size_t base = (slash == std::string::npos) ? 0 : slash + 1;

	// Find the last dot after the base.
	size_t dot = path.rfind('.');
	if (dot == std::string::npos || dot <= base)
		return "";
	return path.substr(dot);
}


}	// namespace pluto
