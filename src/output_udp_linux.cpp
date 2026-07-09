#include "sony_head_tracker/output_udp.hpp"

#include "sony_head_tracker/protocol.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

namespace sony {

struct UdpOutput::Impl {
    int socket{-1};
    sockaddr_in destination{};
    sockaddr_in jsonDestination{};
};

UdpOutput::UdpOutput() : impl_(new Impl) {}

UdpOutput::~UdpOutput() {
    close();
    delete impl_;
}

bool UdpOutput::open(std::string host, std::uint16_t port) {
    close();

    impl_->socket = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (impl_->socket < 0)
        return false;

    impl_->destination = {};
    impl_->destination.sin_family = AF_INET;
    impl_->destination.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &impl_->destination.sin_addr) != 1) {
        close();
        return false;
    }

    impl_->jsonDestination = impl_->destination;
    impl_->jsonDestination.sin_port = htons(static_cast<std::uint16_t>(port + 1));

    port_ = port;
    return true;
}

void UdpOutput::setDeviceLabel(std::wstring_view name) {
    if (name.empty()) {
        deviceJson_ = "null";
        return;
    }

    // Good enough for now: Sony headset names are ASCII in our current path.
    std::string utf8;
    utf8.reserve(name.size());

    for (wchar_t c : name)
        utf8.push_back(c < 128 ? static_cast<char>(c) : '?');

    deviceJson_ = jsonEscapeString(utf8);
}

void UdpOutput::send(const MotionSample& sample) {
    if (!impl_ || impl_->socket < 0)
        return;

    const auto openTrack = toOpenTrackPose(sample);

    ::sendto(
        impl_->socket,
        reinterpret_cast<const char*>(openTrack.data()),
        openTrack.size() * sizeof(double),
        0,
        reinterpret_cast<const sockaddr*>(&impl_->destination),
        sizeof(impl_->destination)
    );

    toJsonTo(jsonBuffer_, sample, deviceJson_);

    ::sendto(
        impl_->socket,
        jsonBuffer_.data(),
        jsonBuffer_.size(),
        0,
        reinterpret_cast<const sockaddr*>(&impl_->jsonDestination),
        sizeof(impl_->jsonDestination)
    );

    ++packetsSent_;
}

void UdpOutput::close() {
    if (impl_ && impl_->socket >= 0) {
        ::close(impl_->socket);
        impl_->socket = -1;
    }
}

} // namespace sony