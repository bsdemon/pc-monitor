#include "metrics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

static double number(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) ? v->valuedouble : NAN;
}

static void text(const cJSON *obj, const char *key, char *out, size_t size)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    snprintf(out, size, "%s", cJSON_IsString(v) ? v->valuestring : "Unavailable");
}

void metrics_empty(metrics_t *m)
{
    memset(m, 0, sizeof(*m));
    m->cpu_load = m->cpu_temp = m->cpu_power = m->cpu_clock = m->cpu_ccd = m->cpu_voltage = NAN;
    m->gpu_load = m->gpu_temp = m->gpu_hot = m->gpu_power = m->gpu_clock = NAN;
    m->gpu_memory_clock = m->gpu_fan = m->gpu_voltage = m->ram_load = NAN;
    m->upload_mbps = m->download_mbps = m->ping_ms = NAN;
    m->vram_used = m->vram_total = m->ram_used = m->ram_total = m->ram_available = NAN;
    snprintf(m->cpu_name, sizeof(m->cpu_name), "Waiting for CPU");
    snprintf(m->gpu_name, sizeof(m->gpu_name), "Waiting for GPU");
}

bool metrics_parse(const char *json, size_t length, metrics_t *m)
{
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return false;
    const cJSON *cpu = cJSON_GetObjectItemCaseSensitive(root, "cpu");
    const cJSON *gpu = cJSON_GetObjectItemCaseSensitive(root, "gpu");
    const cJSON *ram = cJSON_GetObjectItemCaseSensitive(root, "ram");
    const cJSON *stamp = cJSON_GetObjectItemCaseSensitive(root, "measured_at");
    bool ok = number(root, "schema_version") == 1 && cJSON_IsObject(cpu) &&
        cJSON_IsObject(gpu) && cJSON_IsObject(ram) && cJSON_IsString(stamp) &&
        stamp->valuestring[0] && strlen(stamp->valuestring) < sizeof(m->timestamp);
    if (!ok) { cJSON_Delete(root); return false; }
    metrics_empty(m);
    text(root, "computer_id", m->host, sizeof(m->host));
    text(root, "measured_at", m->timestamp, sizeof(m->timestamp));
    text(cpu, "name", m->cpu_name, sizeof(m->cpu_name));
    text(gpu, "name", m->gpu_name, sizeof(m->gpu_name));
    m->cpu_load = number(cpu, "utilization_percent");
    m->cpu_temp = number(cpu, "temperature_celsius");
    m->cpu_power = number(cpu, "power_watts");
    m->cpu_clock = number(cpu, "average_clock_mhz");
    m->cpu_ccd = number(cpu, "ccd_temperature_celsius");
    m->cpu_voltage = number(cpu, "core_voltage_volts");
    double logical = number(cpu, "logical_cores");
    m->logical_cores = isfinite(logical) && logical > 0 && logical <= 1024 ? (int)logical : 0;
    m->gpu_load = number(gpu, "utilization_percent");
    m->gpu_temp = number(gpu, "temperature_celsius");
    m->gpu_hot = number(gpu, "hot_spot_celsius");
    m->gpu_power = number(gpu, "power_watts");
    m->gpu_clock = number(gpu, "core_clock_mhz");
    m->gpu_memory_clock = number(gpu, "memory_clock_mhz");
    m->gpu_fan = number(gpu, "fan_rpm");
    m->gpu_voltage = number(gpu, "core_voltage_volts");
    const cJSON *network = cJSON_GetObjectItemCaseSensitive(root, "network");
    m->upload_mbps = number(network, "upload_bytes_per_second") * 8 / 1000000.0;
    m->download_mbps = number(network, "download_bytes_per_second") * 8 / 1000000.0;
    m->ping_ms = number(network, "ping_ms");
    m->vram_used = number(gpu, "vram_used_bytes");
    m->vram_total = number(gpu, "vram_total_bytes");
    m->ram_load = number(ram, "utilization_percent");
    m->ram_used = number(ram, "used_bytes");
    m->ram_total = number(ram, "total_bytes");
    m->ram_available = number(ram, "available_bytes");
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(cpu, "core_loads")) {
        if (m->cores >= MAX_CORES) break;
        m->core_load[m->cores++] = number(entry, "utilization_percent");
    }
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(ram, "temperatures_celsius")) {
        if (m->dimms >= MAX_DIMMS) break;
        m->dimm_temp[m->dimms++] = number(entry, "value");
    }
    m->stale = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "stale"));
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "hardware_status");
    m->limited = !cJSON_IsString(status) || strcmp(status->valuestring, "ok");
    cJSON_Delete(root);
    return true;
}
