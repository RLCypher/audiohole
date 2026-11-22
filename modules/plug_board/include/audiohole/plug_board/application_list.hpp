#pragma once
#include "audiohole/plug_board/library.hpp"

namespace audiohole::plug_board {
// Display-only comparisons: exact names, ASCII case-insensitive, optional .exe.
// Neither these labels nor host filenames authorize merging or native routing.
std::string normalized_app_name(std::string);
std::vector<std::string> parse_exemptions(const std::string&);
std::vector<std::string> normalize_exemptions(const std::vector<std::string>&);
const std::vector<std::string>& common_host_names();
std::string application_host_name(const ApplicationObservation&);
std::string application_group_name(const ApplicationRecord&);
bool inactive_entry_hidden(const Library&, const ApplicationRecord&);
} // namespace audiohole::plug_board
