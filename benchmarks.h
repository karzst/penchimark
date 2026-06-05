#pragma once
#include <string>
#include <future>
#include "imgui.h"


struct BenchmarkResults {
    // CPU
    bool cpu_testing = false;
    float cpu_progress = 0.0f;
    std::string cpu_status = "Ready";
    int cpu_score = 0;
    std::string cpu_evaluation = "";
    std::future<void> cpu_future;

    // RAM
    bool ram_testing = false;
    float ram_progress = 0.0f;
    std::string ram_status = "Ready";
    int ram_speed_mbps = 0;
    std::string ram_evaluation = "";
    std::future<void> ram_future;

    // GPU
    bool gpu_testing = false;
    float gpu_progress = 0.0f;
    std::string gpu_status = "Ready";
    int gpu_score = 0;
    std::string gpu_evaluation = "";
    std::future<void> gpu_future;

    // Disk
    bool disk_testing = false;
    float disk_progress = 0.0f;
    std::string disk_status = "Ready";
    int disk_read_mbps = 0;
    int disk_write_mbps = 0;
    std::string disk_evaluation = "";
    std::future<void> disk_future;
};


float GetLiveCPUUsage();
float GetLiveGPUUsage();
void RenderCPUBenchmarkModal(BenchmarkResults& results);
void RenderRAMBenchmarkModal(BenchmarkResults& results);
void RenderGPUBenchmarkModal(BenchmarkResults& results);
void RenderDiskBenchmarkModal(BenchmarkResults& results);