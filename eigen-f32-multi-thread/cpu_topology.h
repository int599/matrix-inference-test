#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace matrix_bench {

struct CpuBindingPlan {
  int main_cpu{-1};
  std::vector<int> worker_cpus;
  std::vector<int> compute_cpus;
  std::vector<std::vector<int>> sibling_groups;
  std::size_t physical_core_count{0};
  std::size_t logical_cpu_count{0};
  std::string policy;
};

CpuBindingPlan build_binding_plan(std::size_t thread_count);
void bind_current_thread_to_cpu(int cpu);
std::string join_cpu_list(const std::vector<int>& cpus);
std::string format_sibling_groups(const std::vector<std::vector<int>>& sibling_groups);

}  // namespace matrix_bench
