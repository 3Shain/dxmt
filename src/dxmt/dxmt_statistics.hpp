#pragma once

#include "util_flags.hpp"
#include <array>
#include <chrono>

namespace dxmt {

using clock = std::chrono::high_resolution_clock;

enum class FeatureCompatibility {
    UnsupportedGeometryDraw,
    UnsupportedTessellationOutputPrimitive,
    UnsupportedIndirectTessellationDraw,
    UnsupportedGeometryTessellationDraw,
    UnsupportedDrawAuto,
    UnsupportedPredication,
    UnsupportedStreamOutputAppending,
    UnsupportedMultipleStreamOutput,
  };

struct FrameStatistics {
  Flags<FeatureCompatibility> compatibility_flags;
  uint32_t command_buffer_count = 0;
  uint32_t sync_count = 0;
  clock::duration sync_interval{};
  clock::duration commit_interval{};
  uint32_t render_pass_count = 0;
  uint32_t render_pass_optimized = 0;
  uint32_t clear_pass_count = 0;
  uint32_t clear_pass_optimized = 0;
  uint32_t compute_pass_count = 0;
  uint32_t blit_pass_count = 0;

  void
  reset() {
    compatibility_flags.clrAll();
    command_buffer_count = 0;
    sync_count = 0;
    sync_interval = {};
    commit_interval = {};
    render_pass_count = 0;
    render_pass_optimized = 0;
    clear_pass_count = 0;
    clear_pass_optimized = 0;
    compute_pass_count = 0;
    blit_pass_count = 0;
  };
};

constexpr size_t kFrameStatisticsCount = 16;

class FrameStatisticsContainer {
  std::array<FrameStatistics, kFrameStatisticsCount> frames_;
  FrameStatistics min_;
  FrameStatistics max_;
  FrameStatistics average_;

public:
  FrameStatistics &
  at(uint64_t frame) {
    return frames_[frame % kFrameStatisticsCount];
  }
  const FrameStatistics &
  at(uint64_t frame) const {
    return frames_.at(frame % kFrameStatisticsCount);
  }

  const FrameStatistics &
  min() const {
    return min_;
  }

  const FrameStatistics &
  max() const {
    return max_;
  }

  const FrameStatistics &
  average() const {
    return average_;
  }

  void
  compute(uint64_t current_frame) {
    min_.reset();
    max_.reset();
    average_.reset();
    current_frame = current_frame % kFrameStatisticsCount;
    for(unsigned i = 0; i < kFrameStatisticsCount; i++) {
      if(i == current_frame)
        continue; // deliberately exclude current frame since it 
      min_.command_buffer_count = std::min(min_.command_buffer_count, frames_[i].command_buffer_count);
      min_.sync_count = std::min(min_.sync_count, frames_[i].sync_count);
      min_.commit_interval = std::min(min_.commit_interval, frames_[i].commit_interval);
      min_.sync_interval = std::min(min_.sync_interval, frames_[i].sync_interval);

      max_.command_buffer_count = std::max(max_.command_buffer_count, frames_[i].command_buffer_count);
      max_.sync_count = std::max(max_.sync_count, frames_[i].sync_count);
      max_.commit_interval = std::max(max_.commit_interval, frames_[i].commit_interval);
      max_.sync_interval = std::max(max_.sync_interval, frames_[i].sync_interval);

      average_.command_buffer_count += frames_[i].command_buffer_count;
      average_.sync_count += frames_[i].sync_count;
      average_.commit_interval +=  frames_[i].commit_interval;
      average_.sync_interval +=  frames_[i].sync_interval;
    }
    average_.command_buffer_count /= (kFrameStatisticsCount - 1);
    average_.sync_count /= (kFrameStatisticsCount - 1);
    average_.commit_interval /= (kFrameStatisticsCount - 1);
    average_.sync_interval /= (kFrameStatisticsCount - 1);
  };
};

} // namespace dxmt