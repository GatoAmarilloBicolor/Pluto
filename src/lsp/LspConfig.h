/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef LSP_CONFIG_H
#define LSP_CONFIG_H


#include <string>
#include <vector>


namespace pluto {


struct LspServerConfig {
	std::string				name;		// e.g. "clangd"
	std::string				command;	// e.g. "/boot/system/bin/clangd"
	std::vector<std::string> args;		// e.g. {"--background-index"}
	std::vector<std::string> extensions; // e.g. {".cpp", ".h", ".c"}
};


// Load LSP server configs from:
//   1. data/lsp.conf           (built-in, shipped with package)
//   2. /boot/system/data/pluto/lsp.conf   (packaged install)
//   3. ~/config/settings/pluto/lsp.conf   (user override)
std::vector<LspServerConfig>	LoadLspConfigs();

// Find the server config for a given file extension (e.g. ".cpp").
// Returns NULL if no server is configured for that extension.
const LspServerConfig*			FindLspServer(
									const std::vector<LspServerConfig>& configs,
									const std::string& extension);

// Extract the file extension from a path, including the dot (e.g. ".cpp").
std::string						FileExtension(const std::string& path);


}	// namespace pluto


#endif	// LSP_CONFIG_H
