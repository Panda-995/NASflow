#include "nas_status.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static const char *json_string(cJSON *object, const char *name, const char *fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : fallback;
}

static double json_number(cJSON *object, const char *name, double fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static uint64_t json_u64(cJSON *object, const char *name)
{
    double value = json_number(object, name, 0);
    return value > 0 ? (uint64_t)value : 0;
}

static void copy_string(char *dest, size_t size, const char *value)
{
    if (size == 0) {
        return;
    }
    strlcpy(dest, value ? value : "", size);
}

void nas_status_init(nas_status_t *status)
{
    memset(status, 0, sizeof(*status));
    copy_string(status->schema_version, sizeof(status->schema_version), "1.0");
    copy_string(status->nas.health, sizeof(status->nas.health), "unknown");
    copy_string(status->cpu.health, sizeof(status->cpu.health), "unknown");
    copy_string(status->memory.health, sizeof(status->memory.health), "unknown");
    copy_string(status->network.health, sizeof(status->network.health), "unknown");
}

static void parse_nas(cJSON *root, nas_status_t *status)
{
    cJSON *nas = cJSON_GetObjectItemCaseSensitive(root, "nas");
    if (!cJSON_IsObject(nas)) {
        return;
    }
    copy_string(status->nas.hostname, sizeof(status->nas.hostname), json_string(nas, "hostname", "NAS"));
    copy_string(status->nas.primary_ip, sizeof(status->nas.primary_ip), json_string(nas, "primary_ip", "--"));
    copy_string(status->nas.health, sizeof(status->nas.health), json_string(nas, "health", "unknown"));
    status->nas.uptime_sec = (uint32_t)json_number(nas, "uptime_sec", 0);

    cJSON *alerts = cJSON_GetObjectItemCaseSensitive(nas, "alerts");
    status->nas.alert_count = 0;
    if (cJSON_IsArray(alerts)) {
        cJSON *alert = NULL;
        cJSON_ArrayForEach(alert, alerts) {
            if (status->nas.alert_count >= NAS_MAX_ALERTS) {
                break;
            }
            nas_alert_t *target = &status->nas.alerts[status->nas.alert_count++];
            copy_string(target->level, sizeof(target->level), json_string(alert, "level", "unknown"));
            copy_string(target->message, sizeof(target->message), json_string(alert, "message", ""));
        }
    }
}

static void parse_cpu(cJSON *root, nas_status_t *status)
{
    cJSON *cpu = cJSON_GetObjectItemCaseSensitive(root, "cpu");
    if (!cJSON_IsObject(cpu)) {
        return;
    }
    status->cpu.usage_pct = (float)json_number(cpu, "usage_pct", -1);
    status->cpu.temperature_c = (float)json_number(cpu, "temperature_c", -1000);
    status->cpu.core_count = (int)json_number(cpu, "core_count", 0);
    copy_string(status->cpu.health, sizeof(status->cpu.health), json_string(cpu, "health", "unknown"));
    cJSON *load = cJSON_GetObjectItemCaseSensitive(cpu, "load");
    if (cJSON_IsObject(load)) {
        status->cpu.load_one = (float)json_number(load, "one", 0);
        status->cpu.load_five = (float)json_number(load, "five", 0);
        status->cpu.load_fifteen = (float)json_number(load, "fifteen", 0);
    }
}

static void parse_memory(cJSON *root, nas_status_t *status)
{
    cJSON *memory = cJSON_GetObjectItemCaseSensitive(root, "memory");
    if (!cJSON_IsObject(memory)) {
        return;
    }
    status->memory.total_bytes = json_u64(memory, "total_bytes");
    status->memory.used_bytes = json_u64(memory, "used_bytes");
    status->memory.available_bytes = json_u64(memory, "available_bytes");
    status->memory.cache_bytes = json_u64(memory, "cache_bytes");
    status->memory.swap_total_bytes = json_u64(memory, "swap_total_bytes");
    status->memory.swap_used_bytes = json_u64(memory, "swap_used_bytes");
    status->memory.used_pct = (float)json_number(memory, "used_pct", -1);
    status->memory.swap_used_pct = (float)json_number(memory, "swap_used_pct", -1);
    copy_string(status->memory.health, sizeof(status->memory.health), json_string(memory, "health", "unknown"));
}

static void parse_storage(cJSON *root, nas_status_t *status)
{
    cJSON *storage = cJSON_GetObjectItemCaseSensitive(root, "storage");
    if (!cJSON_IsObject(storage)) {
        return;
    }
    cJSON *pools = cJSON_GetObjectItemCaseSensitive(storage, "pools");
    cJSON *pool = NULL;
    cJSON_ArrayForEach(pool, pools) {
        if (status->storage.pool_count >= NAS_MAX_POOLS) {
            break;
        }
        nas_pool_t *target = &status->storage.pools[status->storage.pool_count++];
        copy_string(target->id, sizeof(target->id), json_string(pool, "id", ""));
        copy_string(target->name, sizeof(target->name), json_string(pool, "name", ""));
        copy_string(target->raid_type, sizeof(target->raid_type), json_string(pool, "raid_type", "unknown"));
        copy_string(target->raid_status, sizeof(target->raid_status), json_string(pool, "raid_status", "unknown"));
        copy_string(target->health, sizeof(target->health), json_string(pool, "health", "unknown"));
        target->total_bytes = json_u64(pool, "total_bytes");
        target->used_bytes = json_u64(pool, "used_bytes");
        target->free_bytes = json_u64(pool, "free_bytes");
        target->used_pct = (float)json_number(pool, "used_pct", -1);
    }

    cJSON *volumes = cJSON_GetObjectItemCaseSensitive(storage, "volumes");
    cJSON *volume = NULL;
    cJSON_ArrayForEach(volume, volumes) {
        if (status->storage.volume_count >= NAS_MAX_VOLUMES) {
            break;
        }
        nas_volume_t *target = &status->storage.volumes[status->storage.volume_count++];
        copy_string(target->id, sizeof(target->id), json_string(volume, "id", ""));
        copy_string(target->name, sizeof(target->name), json_string(volume, "name", ""));
        copy_string(target->pool_id, sizeof(target->pool_id), json_string(volume, "pool_id", ""));
        copy_string(target->filesystem, sizeof(target->filesystem), json_string(volume, "filesystem", ""));
        copy_string(target->health, sizeof(target->health), json_string(volume, "health", "unknown"));
        target->total_bytes = json_u64(volume, "total_bytes");
        target->used_bytes = json_u64(volume, "used_bytes");
        target->free_bytes = json_u64(volume, "free_bytes");
        target->used_pct = (float)json_number(volume, "used_pct", -1);
    }
}

static void parse_drives(cJSON *root, nas_status_t *status)
{
    cJSON *drives = cJSON_GetObjectItemCaseSensitive(root, "drives");
    cJSON *drive = NULL;
    cJSON_ArrayForEach(drive, drives) {
        if (status->drive_count >= NAS_MAX_DRIVES) {
            break;
        }
        nas_drive_t *target = &status->drives[status->drive_count++];
        copy_string(target->id, sizeof(target->id), json_string(drive, "id", ""));
        copy_string(target->bay, sizeof(target->bay), json_string(drive, "bay", ""));
        copy_string(target->type, sizeof(target->type), json_string(drive, "type", ""));
        copy_string(target->model, sizeof(target->model), json_string(drive, "model", ""));
        copy_string(target->smart_status, sizeof(target->smart_status), json_string(drive, "smart_status", "unknown"));
        copy_string(target->health, sizeof(target->health), json_string(drive, "health", "unknown"));
        target->capacity_bytes = json_u64(drive, "capacity_bytes");
        target->temperature_c = (float)json_number(drive, "temperature_c", -1000);
        target->bad_sector_count = (int)json_number(drive, "bad_sector_count", -1);
        target->power_on_hours = (int)json_number(drive, "power_on_hours", -1);
    }
}

static void parse_nvme(cJSON *root, nas_status_t *status)
{
    cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "nvme");
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        if (status->nvme_count >= NAS_MAX_NVME) {
            break;
        }
        nas_nvme_t *target = &status->nvme[status->nvme_count++];
        copy_string(target->id, sizeof(target->id), json_string(item, "id", ""));
        copy_string(target->slot, sizeof(target->slot), json_string(item, "slot", ""));
        copy_string(target->model, sizeof(target->model), json_string(item, "model", ""));
        copy_string(target->cache_state, sizeof(target->cache_state), json_string(item, "cache_state", "unknown"));
        copy_string(target->health, sizeof(target->health), json_string(item, "health", "unknown"));
        target->capacity_bytes = json_u64(item, "capacity_bytes");
        target->used_bytes = json_u64(item, "used_bytes");
        target->temperature_c = (float)json_number(item, "temperature_c", -1000);
        target->available_spare_pct = (int)json_number(item, "available_spare_pct", -1);
        target->percentage_used_pct = (int)json_number(item, "percentage_used_pct", -1);
        target->wear_pct = (int)json_number(item, "wear_pct", -1);
    }
}

static void parse_network(cJSON *root, nas_status_t *status)
{
    cJSON *network = cJSON_GetObjectItemCaseSensitive(root, "network");
    if (!cJSON_IsObject(network)) {
        return;
    }
    status->network.total_rx_bps = json_u64(network, "total_rx_bps");
    status->network.total_tx_bps = json_u64(network, "total_tx_bps");
    copy_string(status->network.health, sizeof(status->network.health), json_string(network, "health", "unknown"));
    cJSON *interfaces = cJSON_GetObjectItemCaseSensitive(network, "interfaces");
    cJSON *iface = NULL;
    cJSON_ArrayForEach(iface, interfaces) {
        if (status->network.interface_count >= NAS_MAX_INTERFACES) {
            break;
        }
        nas_interface_t *target = &status->network.interfaces[status->network.interface_count++];
        copy_string(target->name, sizeof(target->name), json_string(iface, "name", ""));
        copy_string(target->status, sizeof(target->status), json_string(iface, "status", ""));
        cJSON *ips = cJSON_GetObjectItemCaseSensitive(iface, "ip_addresses");
        cJSON *first_ip = cJSON_GetArrayItem(ips, 0);
        copy_string(target->ip, sizeof(target->ip), cJSON_IsString(first_ip) ? first_ip->valuestring : "");
        target->link_speed_mbps = (int)json_number(iface, "link_speed_mbps", -1);
        target->rx_bps = json_u64(iface, "rx_bps");
        target->tx_bps = json_u64(iface, "tx_bps");
        target->rx_errors = (int)json_number(iface, "rx_errors", 0);
        target->tx_errors = (int)json_number(iface, "tx_errors", 0);
        target->rx_dropped = (int)json_number(iface, "rx_dropped", 0);
        target->tx_dropped = (int)json_number(iface, "tx_dropped", 0);
    }
}

static void parse_environment(cJSON *root, nas_status_t *status)
{
    cJSON *env = cJSON_GetObjectItemCaseSensitive(root, "environment");
    if (!cJSON_IsObject(env)) {
        return;
    }
    cJSON *fans = cJSON_GetObjectItemCaseSensitive(env, "fans");
    cJSON *fan = NULL;
    cJSON_ArrayForEach(fan, fans) {
        if (status->environment.fan_count >= NAS_MAX_FANS) {
            break;
        }
        nas_fan_t *target = &status->environment.fans[status->environment.fan_count++];
        copy_string(target->name, sizeof(target->name), json_string(fan, "name", ""));
        copy_string(target->health, sizeof(target->health), json_string(fan, "health", "unknown"));
        target->speed_rpm = (int)json_number(fan, "speed_rpm", -1);
    }
    cJSON *ups = cJSON_GetObjectItemCaseSensitive(env, "ups");
    if (cJSON_IsObject(ups)) {
        status->environment.ups.present = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(ups, "present"));
        copy_string(status->environment.ups.status, sizeof(status->environment.ups.status), json_string(ups, "status", "unknown"));
        copy_string(status->environment.ups.health, sizeof(status->environment.ups.health), json_string(ups, "health", "unknown"));
        status->environment.ups.battery_pct = (int)json_number(ups, "battery_pct", -1);
        status->environment.ups.load_pct = (int)json_number(ups, "load_pct", -1);
        status->environment.ups.runtime_sec = (int)json_number(ups, "runtime_sec", -1);
    }
}

static void parse_workloads(cJSON *root, nas_status_t *status)
{
    cJSON *workloads = cJSON_GetObjectItemCaseSensitive(root, "workloads");
    cJSON *docker = cJSON_GetObjectItemCaseSensitive(workloads, "docker");
    if (!cJSON_IsObject(docker)) {
        return;
    }
    status->workloads.docker.running = (int)json_number(docker, "running", 0);
    status->workloads.docker.stopped = (int)json_number(docker, "stopped", 0);
    status->workloads.docker.unhealthy = (int)json_number(docker, "unhealthy", 0);
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(docker, "containers");
    cJSON *container = NULL;
    cJSON_ArrayForEach(container, containers) {
        if (status->workloads.docker.container_count >= NAS_MAX_CONTAINERS) {
            break;
        }
        nas_container_t *target = &status->workloads.docker.containers[status->workloads.docker.container_count++];
        copy_string(target->name, sizeof(target->name), json_string(container, "name", ""));
        copy_string(target->state, sizeof(target->state), json_string(container, "state", ""));
        copy_string(target->health, sizeof(target->health), json_string(container, "health", "unknown"));
    }
}

static void parse_protection(cJSON *root, nas_status_t *status)
{
    cJSON *protection = cJSON_GetObjectItemCaseSensitive(root, "data_protection");
    if (!cJSON_IsObject(protection)) {
        return;
    }
    status->workloads.backup_count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(protection, "backups"));
    status->workloads.snapshot_count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(protection, "snapshots"));
}

bool nas_status_parse_json(const char *json, nas_status_t *status)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    nas_status_init(status);
    copy_string(status->schema_version, sizeof(status->schema_version), json_string(root, "schema_version", "1.0"));
    copy_string(status->collected_at, sizeof(status->collected_at), json_string(root, "collected_at", ""));
    parse_nas(root, status);
    parse_cpu(root, status);
    parse_memory(root, status);
    parse_storage(root, status);
    parse_drives(root, status);
    parse_nvme(root, status);
    parse_network(root, status);
    parse_environment(root, status);
    parse_workloads(root, status);
    parse_protection(root, status);
    cJSON_Delete(root);
    return true;
}

void nas_format_bytes(uint64_t bytes, char *out, size_t out_size)
{
    const char *unit = "B";
    double value = (double)bytes;
    if (value >= 1099511627776.0) {
        value /= 1099511627776.0;
        unit = "TB";
    } else if (value >= 1073741824.0) {
        value /= 1073741824.0;
        unit = "GB";
    } else if (value >= 1048576.0) {
        value /= 1048576.0;
        unit = "MB";
    }
    snprintf(out, out_size, "%.1f %s", value, unit);
}

void nas_format_bps(uint64_t bps, char *out, size_t out_size)
{
    double value = (double)bps;
    const char *unit = "bps";
    if (value >= 1000000.0) {
        value /= 1000000.0;
        unit = "Mbps";
    } else if (value >= 1000.0) {
        value /= 1000.0;
        unit = "Kbps";
    }
    snprintf(out, out_size, "%.1f %s", value, unit);
}

void nas_format_uptime(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t days = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    if (days > 0) {
        snprintf(out, out_size, "%lud %luh", (unsigned long)days, (unsigned long)hours);
    } else {
        snprintf(out, out_size, "%luh %lum", (unsigned long)hours, (unsigned long)minutes);
    }
}

