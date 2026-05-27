#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAS_MAX_POOLS 4
#define NAS_MAX_VOLUMES 8
#define NAS_MAX_DRIVES 16
#define NAS_MAX_NVME 6
#define NAS_MAX_INTERFACES 6
#define NAS_MAX_FANS 6
#define NAS_MAX_CONTAINERS 10
#define NAS_MAX_ALERTS 8

typedef struct {
    char level[12];
    char message[96];
} nas_alert_t;

typedef struct {
    char hostname[40];
    char primary_ip[48];
    char health[12];
    uint32_t uptime_sec;
    int alert_count;
    nas_alert_t alerts[NAS_MAX_ALERTS];
} nas_identity_t;

typedef struct {
    float usage_pct;
    float temperature_c;
    int core_count;
    float load_one;
    float load_five;
    float load_fifteen;
    char health[12];
} nas_cpu_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t available_bytes;
    uint64_t cache_bytes;
    uint64_t swap_total_bytes;
    uint64_t swap_used_bytes;
    float used_pct;
    float swap_used_pct;
    char health[12];
} nas_memory_t;

typedef struct {
    char id[32];
    char name[48];
    char raid_type[24];
    char raid_status[24];
    char health[12];
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    float used_pct;
} nas_pool_t;

typedef struct {
    char id[32];
    char name[48];
    char pool_id[32];
    char filesystem[16];
    char health[12];
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    float used_pct;
} nas_volume_t;

typedef struct {
    nas_pool_t pools[NAS_MAX_POOLS];
    int pool_count;
    nas_volume_t volumes[NAS_MAX_VOLUMES];
    int volume_count;
} nas_storage_t;

typedef struct {
    char id[24];
    char bay[12];
    char type[12];
    char model[48];
    char smart_status[16];
    char health[12];
    uint64_t capacity_bytes;
    float temperature_c;
    int bad_sector_count;
    int power_on_hours;
} nas_drive_t;

typedef struct {
    char id[24];
    char slot[16];
    char model[48];
    char cache_state[20];
    char health[12];
    uint64_t capacity_bytes;
    uint64_t used_bytes;
    float temperature_c;
    int available_spare_pct;
    int percentage_used_pct;
    int wear_pct;
} nas_nvme_t;

typedef struct {
    char name[24];
    char status[16];
    char ip[48];
    int link_speed_mbps;
    uint64_t rx_bps;
    uint64_t tx_bps;
    int rx_errors;
    int tx_errors;
    int rx_dropped;
    int tx_dropped;
} nas_interface_t;

typedef struct {
    uint64_t total_rx_bps;
    uint64_t total_tx_bps;
    nas_interface_t interfaces[NAS_MAX_INTERFACES];
    int interface_count;
    char health[12];
} nas_network_t;

typedef struct {
    char name[32];
    int speed_rpm;
    char health[12];
} nas_fan_t;

typedef struct {
    bool present;
    char status[16];
    int battery_pct;
    int load_pct;
    int runtime_sec;
    char health[12];
} nas_ups_t;

typedef struct {
    nas_fan_t fans[NAS_MAX_FANS];
    int fan_count;
    nas_ups_t ups;
} nas_environment_t;

typedef struct {
    char name[40];
    char state[16];
    char health[12];
} nas_container_t;

typedef struct {
    int running;
    int stopped;
    int unhealthy;
    nas_container_t containers[NAS_MAX_CONTAINERS];
    int container_count;
} nas_docker_t;

typedef struct {
    nas_docker_t docker;
    int backup_count;
    int snapshot_count;
} nas_workloads_t;

typedef struct {
    char schema_version[8];
    char collected_at[32];
    nas_identity_t nas;
    nas_cpu_t cpu;
    nas_memory_t memory;
    nas_storage_t storage;
    nas_drive_t drives[NAS_MAX_DRIVES];
    int drive_count;
    nas_nvme_t nvme[NAS_MAX_NVME];
    int nvme_count;
    nas_network_t network;
    nas_environment_t environment;
    nas_workloads_t workloads;
} nas_status_t;

void nas_status_init(nas_status_t *status);
bool nas_status_parse_json(const char *json, nas_status_t *status);

void nas_format_bytes(uint64_t bytes, char *out, size_t out_size);
void nas_format_bps(uint64_t bps, char *out, size_t out_size);
void nas_format_uptime(uint32_t seconds, char *out, size_t out_size);

