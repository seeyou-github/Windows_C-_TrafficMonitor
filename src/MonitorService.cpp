#include "MonitorService.h"

#include <iphlpapi.h>

#include <algorithm>
#include <cmath>

namespace {

ULONGLONG FileTimeToUInt64(const FILETIME& file_time) {
    ULARGE_INTEGER value{};
    value.LowPart = file_time.dwLowDateTime;
    value.HighPart = file_time.dwHighDateTime;
    return value.QuadPart;
}

bool IsUsableInterface(const MIB_IF_ROW2& row) {
    if (row.InterfaceAndOperStatusFlags.FilterInterface) {
        return false;
    }
    if (row.OperStatus != IfOperStatusUp) {
        return false;
    }
    if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) {
        return false;
    }
    return true;
}

}  // namespace

MonitorService::MonitorService() = default;
MonitorService::~MonitorService() = default;

SystemSnapshot MonitorService::Sample(bool aggregate_connected_interfaces) {
    SystemSnapshot snapshot{};

    CpuSample cpu_sample{};
    if (ReadCpuSample(cpu_sample)) {
        snapshot.cpu_usage_percent = has_previous_cpu_sample_
                                         ? CalculateCpuUsage(previous_cpu_sample_, cpu_sample)
                                         : 0;
        previous_cpu_sample_ = cpu_sample;
        has_previous_cpu_sample_ = true;
    }

    ReadMemoryUsage(snapshot.memory_usage_percent);

    NetworkSample network_sample{};
    if (ReadNetworkCounters(network_sample, aggregate_connected_interfaces, snapshot.status_text)) {
        if (has_previous_network_sample_ && network_sample.tick_ms > previous_network_sample_.tick_ms &&
            network_sample.aggregate_mode == previous_network_sample_.aggregate_mode &&
            network_sample.source_key == previous_network_sample_.source_key &&
            network_sample.in_octets >= previous_network_sample_.in_octets &&
            network_sample.out_octets >= previous_network_sample_.out_octets) {
            const double seconds =
                static_cast<double>(network_sample.tick_ms - previous_network_sample_.tick_ms) / 1000.0;
            if (seconds > 0.0) {
                snapshot.download_bytes_per_sec =
                    static_cast<double>(network_sample.in_octets - previous_network_sample_.in_octets) / seconds;
                snapshot.upload_bytes_per_sec =
                    static_cast<double>(network_sample.out_octets - previous_network_sample_.out_octets) / seconds;
            }
        }
        previous_network_sample_ = network_sample;
        has_previous_network_sample_ = true;
    }

    return snapshot;
}

bool MonitorService::ReadCpuSample(CpuSample& sample) {
    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};
    if (!::GetSystemTimes(&idle, &kernel, &user)) {
        return false;
    }

    sample.idle = FileTimeToUInt64(idle);
    sample.kernel = FileTimeToUInt64(kernel);
    sample.user = FileTimeToUInt64(user);
    return true;
}

int MonitorService::CalculateCpuUsage(const CpuSample& previous, const CpuSample& current) {
    const ULONGLONG idle_delta = current.idle - previous.idle;
    const ULONGLONG kernel_delta = current.kernel - previous.kernel;
    const ULONGLONG user_delta = current.user - previous.user;
    const ULONGLONG total_delta = kernel_delta + user_delta;
    if (total_delta == 0) {
        return 0;
    }

    const double usage = static_cast<double>(total_delta - idle_delta) * 100.0 /
                         static_cast<double>(total_delta);
    return std::clamp(static_cast<int>(std::round(usage)), 0, 100);
}

bool MonitorService::ReadMemoryUsage(int& memory_percent) {
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (!::GlobalMemoryStatusEx(&statex)) {
        return false;
    }
    memory_percent = static_cast<int>(statex.dwMemoryLoad);
    return true;
}

bool MonitorService::ReadNetworkCounters(NetworkSample& sample,
                                         bool aggregate_connected_interfaces,
                                         std::wstring& status_text) {
    PMIB_IF_TABLE2 table = nullptr;
    if (::GetIfTable2(&table) != NO_ERROR || table == nullptr) {
        status_text.clear();
        return false;
    }

    std::uint64_t total_in = 0;
    std::uint64_t total_out = 0;
    std::uint64_t best_sum = 0;
    std::uint64_t source_key = 0;
    size_t usable_count = 0;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        if (!IsUsableInterface(row)) {
            continue;
        }

        ++usable_count;
        if (aggregate_connected_interfaces) {
            total_in += row.InOctets;
            total_out += row.OutOctets;
            source_key ^= row.InterfaceLuid.Value;
        } else {
            const std::uint64_t traffic_sum = row.InOctets + row.OutOctets;
            if (traffic_sum >= best_sum) {
                best_sum = traffic_sum;
                total_in = row.InOctets;
                total_out = row.OutOctets;
                source_key = row.InterfaceLuid.Value;
            }
        }
    }

    ::FreeMibTable(table);
    sample.in_octets = total_in;
    sample.out_octets = total_out;
    sample.source_key = source_key;
    sample.aggregate_mode = aggregate_connected_interfaces;
    sample.tick_ms = ::GetTickCount64();
    status_text = std::to_wstring(usable_count);
    return true;
}
