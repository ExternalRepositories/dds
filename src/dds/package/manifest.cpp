#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>
#include <dds/util/string.hpp>

#include <range/v3/view/split.hpp>
#include <range/v3/view/split_when.hpp>
#include <range/v3/view/transform.hpp>
#include <semester/walk.hpp>

#include <json5/parse_data.hpp>

using namespace dds;

namespace {

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

package_manifest parse_json(const json5::data& data, std::string_view fpath) {
    package_manifest ret;

    using namespace semester::walk_ops;
    auto push_depends_obj_kv = [&](std::string key, auto&& dat) {
        dependency pending_dep;
        if (!dat.is_string()) {
            return walk.reject("Dependency object values should be strings");
        }
        try {
            auto       rng = semver::range::parse_restricted(dat.as_string());
            dependency dep{std::move(key), {rng.low(), rng.high()}};
            ret.dependencies.push_back(std::move(dep));
        } catch (const semver::invalid_range&) {
            throw_user_error<errc::invalid_version_range_string>(
                "Invalid version range string '{}' in dependency declaration for "
                "'{}'",
                dat.as_string(),
                key);
        }
        return walk.accept;
    };

    walk(data,
         require_obj{"Root of package manifest should be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.pkg_id.name}},
             required_key{"namespace",
                          "A string 'namespace' is a required ",
                          require_str{"'namespace' must be a string"},
                          put_into{ret.namespace_}},
             required_key{"version",
                          "A 'version' string is requried",
                          require_str{"'version' must be a string"},
                          put_into{ret.pkg_id.version,
                                   [](std::string s) { return semver::version::parse(s); }}},
             if_key{"depends",
                    [&](auto&& dat) {
                        if (dat.is_object()) {
                            dds_log(warn,
                                    "{}: Using a JSON object for 'depends' is deprecated. Use an "
                                    "array of strings instead.",
                                    fpath);
                            return mapping{push_depends_obj_kv}(dat);
                        } else if (dat.is_array()) {
                            return for_each{put_into{std::back_inserter(ret.dependencies),
                                                     [](const std::string& depstr) {
                                                         return dependency::parse_depends_string(
                                                             depstr);
                                                     }}}(dat);
                        } else {
                            return walk.reject(
                                "'depends' should be an array of dependency strings");
                        }
                    }},
             if_key{"test_driver",
                    require_str{"'test_driver' must be a string"},
                    put_into{ret.test_driver,
                             [](std::string const& td_str) {
                                 if (td_str == "Catch-Main") {
                                     return test_lib::catch_main;
                                 } else if (td_str == "Catch") {
                                     return test_lib::catch_;
                                 } else {
                                     auto dym = *did_you_mean(td_str, {"Catch-Main", "Catch"});
                                     throw_user_error<errc::unknown_test_driver>(
                                         "Unknown 'test_driver' '{}' (Did you mean '{}'?)",
                                         td_str,
                                         dym);
                                 }
                             }}},
         });
    return ret;
}

}  // namespace

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);
    try {
        return parse_json(data, fpath.string());
    } catch (const semester::walk_error& e) {
        throw_user_error<errc::invalid_pkg_manifest>(e.what());
    }
}

std::optional<fs::path> package_manifest::find_in_directory(path_ref dirpath) {
    auto cands = {
        "package.json5",
        "package.jsonc",
        "package.json",
    };
    for (auto c : cands) {
        auto cand = dirpath / c;
        if (fs::is_regular_file(cand)) {
            return cand;
        }
    }

    return std::nullopt;
}

std::optional<package_manifest> package_manifest::load_from_directory(path_ref dirpath) {
    auto found = find_in_directory(dirpath);
    if (!found.has_value()) {
        return std::nullopt;
    }
    return load_from_file(*found);
}
