#pragma once

#include "Tool.h"

namespace pluto {

std::unique_ptr<Tool> make_read_file_tool();
std::unique_ptr<Tool> make_write_file_tool();
std::unique_ptr<Tool> make_edit_file_tool();
std::unique_ptr<Tool> make_bash_tool();
std::unique_ptr<Tool> make_grep_tool();
std::unique_ptr<Tool> make_glob_tool();

}  // namespace pluto
