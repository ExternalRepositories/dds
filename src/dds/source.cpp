#include "./source.hpp"

#include <dds/util/string.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <vector>

using namespace dds;

std::optional<source_kind> dds::infer_source_kind(path_ref p) noexcept {
    static std::vector<std::string_view> header_exts = {
        ".h",
        ".H",
        ".H++",
        ".h++",
        ".hh",
        ".hpp",
        ".hxx",
        ".inl",
    };
    static std::vector<std::string_view> source_exts = {
        ".C",
        ".c",
        ".c++",
        ".cc",
        ".cpp",
        ".cxx",
    };
    auto leaf = p.filename();

    auto ext_found
        = std::lower_bound(header_exts.begin(), header_exts.end(), p.extension(), std::less<>());
    if (ext_found != header_exts.end() && *ext_found == p.extension()) {
        return source_kind::header;
    }

    ext_found
        = std::lower_bound(source_exts.begin(), source_exts.end(), p.extension(), std::less<>());
    if (ext_found == source_exts.end() || *ext_found != p.extension()) {
        return std::nullopt;
    }

    if (ends_with(p.stem().string(), ".test")) {
        return source_kind::test;
    }

    if (ends_with(p.stem().string(), ".main")) {
        return source_kind::app;
    }

    return source_kind::source;
}

std::optional<source_file> source_file::from_path(path_ref path, path_ref base_path) noexcept {
    auto kind = infer_source_kind(path);
    if (!kind.has_value()) {
        return std::nullopt;
    }

    return source_file{path, base_path, *kind};
}
