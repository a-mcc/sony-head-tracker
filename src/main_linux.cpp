#include <hidapi/hidapi.h>

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

constexpr unsigned short SonyVid = 0x054c;
constexpr unsigned short Xm6Pid = 0x0f8a;

int main() {
    if (hid_init() != 0) {
        std::cerr << "hid_init() failed\n";
        return 1;
    }

    hid_device_info* list = hid_enumerate(SonyVid, Xm6Pid);

    std::string path;

    std::cout << "Enumerating Sony devices...\n\n";

    for (hid_device_info* d = list; d; d = d->next) {
        std::cout << "----------------------------------------\n";
        std::cout << "VID: 0x" << std::hex << std::setw(4) << std::setfill('0')
                  << d->vendor_id
                  << " PID: 0x"
                  << std::setw(4)
                  << d->product_id
                  << std::dec << "\n";

        if (d->manufacturer_string)
            std::wcout << L"Manufacturer: " << d->manufacturer_string << L'\n';

        if (d->product_string)
            std::wcout << L"Product: " << d->product_string << L'\n';

        std::cout << "Usage Page: 0x" << std::hex << d->usage_page << '\n';
        std::cout << "Usage:      0x" << d->usage << std::dec << '\n';
        std::cout << "Interface:  " << d->interface_number << '\n';
        std::cout << "Path:       " << (d->path ? d->path : "(null)") << "\n\n";

        if (d->usage_page == 0x20 && d->usage == 0xE1) {
            std::cout << "*** Android Head Tracker collection found ***\n";
            path = d->path;
        }
    }

    hid_free_enumeration(list);

    if (path.empty()) {
        std::cerr << "\nNo Android Head Tracker collection found.\n";
        hid_exit();
        return 1;
    }

    std::cout << "\nOpening:\n" << path << "\n\n";

    hid_device* device = hid_open_path(path.c_str());

    if (!device) {
        const wchar_t* err = hid_error(nullptr);

        if (err)
            std::wcerr << L"hid_open_path failed: " << err << L'\n';
        else
            std::wcerr << L"hid_open_path failed (no error string)\n";

        hid_exit();
        return 1;
    }

    std::cout << "Opened successfully!\n";

    std::array<unsigned char, 2> feature = {0x01, 0x03};

    int result = hid_send_feature_report(device, feature.data(), feature.size());

    std::cout << "hid_send_feature_report() returned " << result << "\n";

    std::array<unsigned char, 14> report{};

    while (true) {
        int bytes = hid_read(device, report.data(), report.size());

        if (bytes < 0) {
            std::wcerr << L"Read failed: " << hid_error(device) << L'\n';
            break;
        }

        if (bytes == 0)
            continue;

        std::cout << std::hex << std::uppercase << std::setfill('0');

        for (int i = 0; i < bytes; ++i)
            std::cout << std::setw(2) << static_cast<int>(report[i]) << ' ';

        std::cout << '\n';
    }

    hid_close(device);
    hid_exit();

    return 0;
}