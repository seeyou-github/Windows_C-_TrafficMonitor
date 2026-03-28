#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <netioapi.h>

#include <cstdint>
#include <string>

struct SystemSnapshot {
    double upload_bytes_per_sec = 0.0;
    double download_bytes_per_sec = 0.0;
    int cpu_usage_percent = 0;
    int memory_usage_percent = 0;
    std::wstring status_text;
};

class MonitorService {
public:
    MonitorService();
    ~MonitorService();

    SystemSnapshot Sample(bool aggregate_connected_interfaces);

private:
    struct CpuSample {
        ULONGLONG idle = 0;
        ULONGLONG kernel = 0;
        ULONGLONG user = 0;
    };

    struct NetworkSample {
        std::uint64_t in_octets = 0;
        std::uint64_t out_octets = 0;
        ULONGLONG tick_ms = 0;
    };

    CpuSample previous_cpu_sample_{};
    bool has_previous_cpu_sample_ = false;
    NetworkSample previous_network_sample_{};
    bool has_previous_network_sample_ = false;

    static bool ReadCpuSample(CpuSample& sample);
    static int CalculateCpuUsage(const CpuSample& previous, const CpuSample& current);
    static bool ReadMemoryUsage(int& memory_percent);
    static bool ReadNetworkCounters(NetworkSample& sample,
                                    bool aggregate_connected_interfaces,
                                    std::wstring& status_text);
};
