#include "sony_head_tracker/hid_backend.hpp"
#include "sony_head_tracker/hid_descriptor.hpp"
#include "sony_head_tracker/hid_usages.hpp"

#include <hidapi/hidapi.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sony {
namespace {

constexpr unsigned short kSonyVid = 0x054c;
constexpr std::size_t kInputReportBytes = 14;
constexpr std::size_t kFeatureReportBytes = 2;

std::wstring widen(const char* s) {
    if (!s) return {};
    std::wstring out;
    while (*s) out.push_back(static_cast<unsigned char>(*s++));
    return out;
}

std::string narrow(const std::wstring& s) {
    std::string out;
    for (wchar_t c : s) out.push_back(static_cast<char>(c));
    return out;
}

DescriptorField xm6RotationField() {
    DescriptorField f;
    f.usagePage = kSensorPage;
    f.usage = kRotation;
    f.reportId = 1;
    f.reportCount = 3;
    f.bitSize = 16;
    f.logicalMin = -32767;
    f.logicalMax = 32767;
    f.physicalMin = -314159264;
    f.physicalMax = 314159265;
    f.unitExponent = -8;
    f.unit = 0;
    f.dataIndex = 0;
    f.feature = false;
    return f;
}

} // namespace

struct HidBackend::Context {
    hid_device* device{};
    RawCallback raw;
    SampleCallback sample;
    DescriptorField rotationField{xm6RotationField()};
    std::vector<double> decodedRotation;
    std::chrono::steady_clock::time_point rateStart{std::chrono::steady_clock::now()};
    std::uint64_t rateCount{};
    double rate{};

    ~Context() {
        if (device) hid_close(device);
    }

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
};

HidBackend::HidBackend() { hid_init(); }

HidBackend::~HidBackend() {
    disconnect();
    hid_exit();
}

std::vector<DeviceInfo> HidBackend::enumerate(bool) {
    std::vector<DeviceInfo> devices;
    hid_device_info* list = hid_enumerate(kSonyVid, 0);

    for (hid_device_info* d = list; d; d = d->next) {
        if (d->usage_page != kSensorPage || d->usage != kOtherCustom)
            continue;

        DeviceInfo info;
        info.path = widen(d->path);
        info.product = d->product_string ? d->product_string : L"";
        info.manufacturer = d->manufacturer_string ? d->manufacturer_string : L"";
        info.vendorId = d->vendor_id;
        info.productId = d->product_id;
        info.usagePage = d->usage_page;
        info.usage = d->usage;
        info.inputReportBytes = kInputReportBytes;
        info.featureReportBytes = kFeatureReportBytes;
        info.sensorDescription = std::string(kMarker);
        info.androidHeadTracker = true;
        info.fields.push_back(xm6RotationField());
        devices.push_back(std::move(info));
    }

    hid_free_enumeration(list);
    return devices;
}

bool HidBackend::connect(const DeviceInfo& device, RawCallback raw, SampleCallback sample) {
    disconnect();

    auto ctx = std::make_unique<Context>();
    ctx->raw = std::move(raw);
    ctx->sample = std::move(sample);

    const auto path = narrow(device.path);
    ctx->device = hid_open_path(path.c_str());

    if (!ctx->device) {
        std::wcerr << L"hid_open_path failed: " << hid_error(nullptr) << L'\n';
        return false;
    }

    std::array<unsigned char, kFeatureReportBytes> feature = {0x01, 0x03};
    const int featureResult = hid_send_feature_report(ctx->device, feature.data(), feature.size());

    if (featureResult < 0) {
        std::wcerr << L"hid_send_feature_report failed: " << hid_error(ctx->device) << L'\n';
        return false;
    }

    context_ = std::move(ctx);
    running_ = true;

    reader_ = std::jthread([this](std::stop_token stop) {
        auto* c = context_.get();
        std::array<unsigned char, kInputReportBytes> report{};

        while (!stop.stop_requested()) {
            const int bytes = hid_read_timeout(c->device, report.data(), report.size(), 100);

            if (bytes < 0) {
                std::wcerr << L"hid_read_timeout failed: " << hid_error(c->device) << L'\n';
                break;
            }

            if (bytes == 0)
                continue;

            std::vector<std::uint8_t> packet(report.begin(), report.begin() + bytes);

            if (c->raw)
                c->raw(packet);

            if (bytes >= 7 && report[0] == 0x01 && c->sample) {
                decodePackedDescriptorValuesInto(
                    c->decodedRotation,
                    std::span<const std::uint8_t>(report.data() + 1, 6),
                    c->rotationField
                );

                if (c->decodedRotation.size() < 3)
                    continue;

                MotionSample s;
                s.receivedAt = std::chrono::steady_clock::now();
                s.rotationVector = {
                    c->decodedRotation[0],
                    c->decodedRotation[1],
                    c->decodedRotation[2]
                };

                ++c->rateCount;
                const auto elapsed = std::chrono::duration<double>(s.receivedAt - c->rateStart).count();

                if (elapsed >= 1.0) {
                    c->rate = static_cast<double>(c->rateCount) / elapsed;
                    c->rateCount = 0;
                    c->rateStart = s.receivedAt;
                }

                s.packetsPerSecond = c->rate;
                s.receiveLatencyMs = -1.0;
                c->sample(std::move(s));
            }
        }

        running_ = false;
    });

    return true;
}

void HidBackend::disconnect() {
    running_ = false;

    if (reader_.joinable()) {
        reader_.request_stop();
        reader_.join();
    }

    context_.reset();
}

std::wstring hexDump(const std::vector<std::uint8_t>& bytes) {
    std::wostringstream out;
    out << std::hex << std::uppercase << std::setfill(L'0');

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) out << L' ';
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }

    return out.str();
}

} // namespace sony