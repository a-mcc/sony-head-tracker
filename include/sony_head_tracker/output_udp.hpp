#pragma once

#include "sony_head_tracker/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace sony {

    class UdpOutput {
    public:
        UdpOutput();
        ~UdpOutput();
        UdpOutput(const UdpOutput&) = delete;
        UdpOutput& operator=(const UdpOutput&) = delete;

        bool open(std::string host, std::uint16_t port);
        void setDeviceLabel(std::wstring_view name);
        void send(const MotionSample& sample);
        void close();

        [[nodiscard]] std::uint64_t packetsSent() const { return packetsSent_; }
        [[nodiscard]] std::uint16_t port() const { return port_; }

    private:
        struct Impl;
        Impl* impl_{};
        std::string deviceJson_{"null"};
        std::string jsonBuffer_;
        std::uint64_t packetsSent_{};
        std::uint16_t port_{};
    };

} // namespace sony