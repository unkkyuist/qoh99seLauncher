#pragma once
#include <filesystem>
#include <cstddef>

namespace qoh_runtime {
// Existing ddraw.ini is preserved; replacements are backed up before any writes.
std::size_t Prepare(const std::filesystem::path& gameDir, bool gameRunning);
}
