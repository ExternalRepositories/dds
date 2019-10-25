#include "./deps.hpp"

#include <dds/build/sroot.hpp>
#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/util/string.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <map>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    const auto str_begin = str.data();
    auto       str_iter  = str_begin;
    const auto str_end   = str_iter + str.size();

    while (str_iter != str_end && !std::isspace(*str_iter)) {
        ++str_iter;
    }

    auto name        = trim(std::string_view(str_begin, str_iter - str_begin));
    auto version_str = trim(std::string_view(str_iter, str_end - str_iter));

    semver::version version;
    try {
        version = semver::version::parse(version_str);
    } catch (const semver::invalid_version&) {
        throw std::runtime_error(
            fmt::format("Invalid version string '{}' in dependency declaration '{}' (Should be a "
                        "semver string. See https://semver.org/ for info)",
                        version_str,
                        str));
    }
    return dependency{std::string(name), version};
}

std::vector<sdist> dds::find_dependencies(const repository& repo, const dependency& dep) {
    std::vector<sdist> acc;
    detail::do_find_deps(repo, dep, acc);
    return acc;
}

void detail::do_find_deps(const repository& repo, const dependency& dep, std::vector<sdist>& sd) {
    auto sdist_opt = repo.get_sdist(dep.name, dep.version.to_string());
    if (!sdist_opt) {
        throw std::runtime_error(
            fmt::format("Unable to find dependency to satisfy requirement: {} {}",
                        dep.name,
                        dep.version.to_string()));
    }
    sdist& new_sd = *sdist_opt;
    for (const auto& inner_dep : new_sd.manifest.dependencies) {
        do_find_deps(repo, inner_dep, sd);
    }
    auto insert_point = std::partition_point(sd.begin(), sd.end(), [&](const sdist& cand) {
        return cand.path < new_sd.path;
    });
    if (insert_point != sd.end() && insert_point->manifest.name == new_sd.manifest.name) {
        if (insert_point->manifest.version != new_sd.manifest.version) {
            assert(false && "Version conflict resolution not implemented yet");
            std::terminate();
        }
        return;
    }
    sd.insert(insert_point, std::move(new_sd));
}

using sdist_index_type = std::map<std::string, std::reference_wrapper<const sdist>>;

namespace {

void add_dep_includes(shared_compile_file_rules& rules,
                      const package_manifest&    man,
                      const sdist_index_type&    sd_idx) {
    for (const dependency& dep : man.dependencies) {
        auto found = sd_idx.find(dep.name);
        if (found == sd_idx.end()) {
            throw std::runtime_error(
                fmt::format("Unable to resolve dependency '{}' (required by '{}')",
                            dep.name,
                            man.name));
        }
        add_dep_includes(rules, found->second.get().manifest, sd_idx);
        rules.include_dirs().push_back(sroot{found->second.get().path}.public_include_dir());
    }
}

void add_sdist_to_dep_plan(build_plan& plan, const sdist& sd, const sdist_index_type& sd_idx) {
    auto                      root       = dds::sroot{sd.path};
    shared_compile_file_rules comp_rules = root.base_compile_rules();
    add_dep_includes(comp_rules, sd.manifest, sd_idx);
    sroot_build_params params;
    params.main_name     = sd.manifest.name;
    params.compile_rules = comp_rules;
    plan.add_sroot(root, params);
}

}  // namespace

build_plan dds::create_deps_build_plan(const std::vector<sdist>& deps) {
    auto sd_idx = deps | ranges::views::transform([](const auto& sd) {
                      return std::pair(sd.manifest.name, std::cref(sd));
                  })
        | ranges::to<sdist_index_type>();

    build_plan plan;
    for (const sdist& sd : deps) {
        spdlog::info("Recording dependency: {}", sd.manifest.name);
        add_sdist_to_dep_plan(plan, sd, sd_idx);
    }
    return plan;
}
