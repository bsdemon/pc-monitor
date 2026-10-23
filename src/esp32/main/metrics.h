#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_CORES 32
#define MAX_DIMMS 8

typedef struct {
    char host[64], timestamp[64], cpu_name[96], gpu_name[96];
    float cpu_load, cpu_temp, cpu_power, cpu_clock, cpu_ccd, cpu_voltage;
    float gpu_load, gpu_temp, gpu_hot, gpu_power, gpu_clock, gpu_memory_clock, gpu_fan, gpu_voltage;
    float upload_mbps, download_mbps, ping_ms;
    double vram_used, vram_total, ram_used, ram_total, ram_available;
    float ram_load, core_load[MAX_CORES], dimm_temp[MAX_DIMMS];
    int cores, dimms, logical_cores;
    bool stale, limited;
} metrics_t;

bool metrics_parse(const char *json, size_t length, metrics_t *result);
void metrics_empty(metrics_t *result);
