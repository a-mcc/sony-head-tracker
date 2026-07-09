#include "sony_head_tracker/hid_backend.hpp"
#include "sony_head_tracker/orientation.hpp"
#include "sony_head_tracker/output_udp.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    constexpr std::uint16_t port = 4242;

    sony::HidBackend hid;

    auto devices = hid.enumerate();

    if (devices.empty()) {
        std::wcerr << L"No Android Head Tracker HID device found\n";
        return 1;
    }

    auto device = devices.front();

    std::wcout << L"Tracking: "
               << (device.product.empty() ? device.path : device.product)
               << L'\n';

    sony::UdpOutput udp;

    if (!udp.open("127.0.0.1", port)) {
        std::wcerr << L"Could not open UDP output\n";
        return 2;
    }

    udp.setDeviceLabel(device.product.empty() ? device.path : device.product);

    sony::FilterConfig config;
    sony::OrientationFilter filter(config);

    std::wcout << L"Streaming:\n"
               << L"  OpenTrack doubles -> UDP 127.0.0.1:" << port << L'\n'
               << L"  JSON telemetry    -> UDP 127.0.0.1:" << port + 1 << L'\n';

    if (!hid.connect(
            device,
            {},
            [&](sony::MotionSample sample) {
                auto out = filter.process(std::move(sample));
                udp.send(out);

                std::wcout << L"\rYPR "
                           << out.euler.yaw << L" "
                           << out.euler.pitch << L" "
                           << out.euler.roll << L"  "
                           << out.packetsPerSecond << L" pps      "
                           << std::flush;
            }
        )) {
        std::wcerr << L"Failed to connect\n";
        return 3;
        }

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}