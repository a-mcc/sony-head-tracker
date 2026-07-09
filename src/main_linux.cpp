#include "sony_head_tracker/hid_backend.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    sony::HidBackend hid;

    auto devices = hid.enumerate();

    if (devices.empty()) {
        std::wcerr << L"No Android Head Tracker HID device found\n";
        return 1;
    }

    const auto& device = devices.front();

    std::wcout << L"Tracking: "
               << (device.product.empty() ? device.path : device.product)
               << L'\n';

    if (!hid.connect(
            device,
            [](const std::vector<std::uint8_t>& packet) {
                std::wcout << L"RAW " << sony::hexDump(packet) << L'\n';
            },
            [](sony::MotionSample sample) {
                const auto& r = sample.rotationVector;

                std::wcout << L"SAMPLE "
                           << r[0] << L" "
                           << r[1] << L" "
                           << r[2] << L" pps="
                           << sample.packetsPerSecond
                           << L'\n';
            }
        )) {
        std::wcerr << L"Failed to connect\n";
        return 2;
        }

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}