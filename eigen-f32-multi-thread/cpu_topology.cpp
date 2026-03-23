#include "cpu_topology.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sched.h>
#include <pthread.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string read_trimmed_file(const std::filesystem::path& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    throw std::runtime_error("failed to open " + path.string());
  }

  std::string value;
  std::getline(ifs, value);
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ')) {
    value.pop_back();
  }
  return value;
}

std::vector<int> parse_cpu_list(const std::string& text) {
  std::vector<int> cpus;
  std::stringstream ss(text);
  std::string token;

  while (std::getline(ss, token, ',')) {
    auto dash_pos = token.find('-');
    if (dash_pos == std::string::npos) {
      cpus.push_back(std::stoi(token));
      continue;
    }

    int begin = std::stoi(token.substr(0, dash_pos));
    int end = std::stoi(token.substr(dash_pos + 1));
    if (end < begin) {
      throw std::runtime_error("invalid cpu range " + token);
    }
    for (int cpu = begin; cpu <= end; ++cpu) {
      cpus.push_back(cpu);
    }
  }

  std::sort(cpus.begin(), cpus.end());
  cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
  return cpus;
}

std::vector<std::vector<int>> discover_sibling_groups() {
  std::set<std::vector<int>> unique_groups;
  const std::filesystem::path cpu_root{"/sys/devices/system/cpu"};

  for (const auto& entry : std::filesystem::directory_iterator(cpu_root)) {
    if (!entry.is_directory()) {
      continue;
    }

    const auto name = entry.path().filename().string();
    if (!name.starts_with("cpu") || name.size() <= 3) {
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(name[3]))) {
      continue;
    }

    const auto sibling_path = entry.path() / "topology" / "thread_siblings_list";
    if (!std::filesystem::exists(sibling_path)) {
      continue;
    }
    unique_groups.insert(parse_cpu_list(read_trimmed_file(sibling_path)));
  }

  if (unique_groups.empty()) {
    throw std::runtime_error("no CPU sibling groups discovered");
  }

  return {unique_groups.begin(), unique_groups.end()};
}

}  // namespace

namespace matrix_bench {

CpuBindingPlan build_binding_plan(std::size_t thread_count) {
  auto sibling_groups = discover_sibling_groups();
  std::sort(
    sibling_groups.begin(),
    sibling_groups.end(),
    [](const auto& lhs, const auto& rhs) { return lhs.front() < rhs.front(); });

  std::vector<int> primary_cpus;
  std::vector<int> sibling_cpus;
  for (const auto& group : sibling_groups) {
    primary_cpus.push_back(group.front());
    for (std::size_t idx = 1; idx < group.size(); ++idx) {
      sibling_cpus.push_back(group[idx]);
    }
  }

  std::vector<int> ordered_compute_cpus = primary_cpus;
  ordered_compute_cpus.insert(
    ordered_compute_cpus.end(), sibling_cpus.begin(), sibling_cpus.end());

  if (thread_count == 0) {
    throw std::runtime_error("thread count must be positive");
  }
  if (thread_count > ordered_compute_cpus.size()) {
    throw std::runtime_error("requested thread count exceeds logical CPU count");
  }

  CpuBindingPlan plan;
  plan.policy = thread_count <= primary_cpus.size()
                  ? "physical_cores_only_main_included"
                  : "physical_first_then_hyper_threads_main_included";
  plan.main_cpu = ordered_compute_cpus.front();
  plan.compute_cpus.assign(
    ordered_compute_cpus.begin(), ordered_compute_cpus.begin() + thread_count);
  plan.physical_core_count = primary_cpus.size();
  plan.logical_cpu_count = ordered_compute_cpus.size();
  if (thread_count > 1) {
    plan.worker_cpus.assign(plan.compute_cpus.begin() + 1, plan.compute_cpus.end());
  }
  plan.sibling_groups = std::move(sibling_groups);
  return plan;
}

void bind_current_thread_to_cpu(int cpu) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(cpu, &cpu_set);

  const int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  if (rc != 0) {
    throw std::runtime_error("pthread_setaffinity_np failed");
  }
}

std::string join_cpu_list(const std::vector<int>& cpus) {
  std::ostringstream oss;
  for (std::size_t idx = 0; idx < cpus.size(); ++idx) {
    if (idx != 0) {
      oss << ',';
    }
    oss << cpus[idx];
  }
  return oss.str();
}

std::string format_sibling_groups(const std::vector<std::vector<int>>& sibling_groups) {
  std::ostringstream oss;
  for (std::size_t group_idx = 0; group_idx < sibling_groups.size(); ++group_idx) {
    if (group_idx != 0) {
      oss << ';';
    }
    oss << join_cpu_list(sibling_groups[group_idx]);
  }
  return oss.str();
}

}  // namespace matrix_bench
