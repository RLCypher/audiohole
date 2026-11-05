#include "audiohole/test_suite.hpp"
#include <miniz.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace audiohole::scanner {
namespace {
struct Zip {
  mz_zip_archive value{};
  bool initialized = false;
  ~Zip() { if (initialized) mz_zip_writer_end(&value); }
};
size_t write_bytes(void* opaque, mz_uint64 offset, const void* buffer, size_t count) {
  try {
    auto& stream = *static_cast<std::ofstream*>(opaque);
    stream.seekp(static_cast<std::streamoff>(offset));
    stream.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(count));
    return stream ? count : 0;
  } catch (...) { return 0; }
}
size_t read_bytes(void* opaque, mz_uint64 offset, void* buffer, size_t count) {
  try {
    auto& stream = *static_cast<std::ifstream*>(opaque);
    stream.seekg(static_cast<std::streamoff>(offset));
    stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(count));
    return static_cast<size_t>(stream.gcount());
  } catch (...) { return 0; }
}
}
void zip_results(const fs::path& directory, const fs::path& archive) {
  if (!fs::is_directory(directory) || fs::is_symlink(fs::symlink_status(directory)))
    throw std::runtime_error("ZIP source must be a real directory");
  if (fs::exists(archive)) throw std::runtime_error("ZIP destination already exists");
  const auto partial = fs::path(archive.native() + fs::path(".partial").native());
  if (fs::exists(partial)) throw std::runtime_error("Partial ZIP destination already exists");
  auto relative = fs::absolute(archive).lexically_normal().lexically_relative(fs::absolute(directory).lexically_normal());
  if (!relative.empty() && *relative.begin() != "..") throw std::runtime_error("ZIP must be outside its source directory");
  std::vector<fs::path> files;
  std::uint64_t total = 0;
  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    const auto state = entry.symlink_status();
    if (fs::is_symlink(state)) throw std::runtime_error("Symlinks are not included in test archives");
    if (!fs::is_regular_file(state)) continue;
    total += entry.file_size();
    if (total > 2ULL * 1024 * 1024 * 1024 || files.size() >= 60000)
      throw std::runtime_error("Result archive exceeds the 2 GiB/60000-file budget");
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  try {
    std::ofstream output(partial, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot create result ZIP");
    Zip zip; zip.value.m_pIO_opaque = &output; zip.value.m_pWrite = write_bytes;
    if (!mz_zip_writer_init(&zip.value, 0)) throw std::runtime_error("Cannot initialize result ZIP");
    zip.initialized = true;
    std::uint64_t actual_total = 0;
    for (const auto& file : files) {
      auto name = file.lexically_relative(directory).generic_u8string();
      const std::string utf8(reinterpret_cast<const char*>(name.data()), name.size());
      if (utf8.empty() || utf8.size() > 60000 || utf8.starts_with("../")) throw std::runtime_error("Invalid result archive filename");
      const auto size = fs::file_size(file);
      actual_total += size;
      if (actual_total > 2ULL * 1024 * 1024 * 1024) throw std::runtime_error("Result files grew beyond the ZIP input budget");
      std::ifstream input(file, std::ios::binary);
      if (!input || !mz_zip_writer_add_read_buf_callback(&zip.value, utf8.c_str(), read_bytes, &input, size,
          nullptr, nullptr, 0, MZ_BEST_SPEED, nullptr, 0, nullptr, 0) || fs::file_size(file) != size)
        throw std::runtime_error("Cannot archive result file: " + path_utf8(file));
    }
    if (!mz_zip_writer_finalize_archive(&zip.value)) throw std::runtime_error("Cannot finalize result ZIP");
    mz_zip_writer_end(&zip.value); zip.initialized = false;
    output.flush(); if (!output) throw std::runtime_error("Cannot flush result ZIP");
    output.close(); if (output.fail()) throw std::runtime_error("Cannot close result ZIP");
#ifdef _WIN32
    if (!MoveFileExW(partial.c_str(), archive.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot publish result ZIP");
#else
    fs::create_hard_link(partial, archive); fs::remove(partial);
#endif
  } catch (...) { std::error_code ignored; fs::remove(partial, ignored); throw; }
}
}
