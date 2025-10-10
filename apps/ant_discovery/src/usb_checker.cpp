#include <iostream>
static bool check_usb_is_available();

#ifdef __APPLE__
#include <string>

static bool check_usb_is_available()
{
    // Use 'ps aux' and grep for Garmin
    FILE* pipe = popen("ps aux | grep -i garmin | grep -v grep", "r");
    if (!pipe) return false;

    char buffer[256];
    bool found = false;
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        // Basic but effective check: "Garmin Express" or "Garmin" in path
        if (std::string line(buffer); line.find("Garmin") != std::string::npos || line.find("garmin") !=
            std::string::npos)
        {
            found = true;
            break;
        }
    }
    pclose(pipe);
    if (found)
    {
        std::cerr << "\033[1;31m[WARNING]\033[0m: A Garmin-related process (like Garmin Express) is running.\n"
            << "→ Please close all Garmin apps and background services, then unplug and replug your ANT stick.\n"
            << "Only one program can use the ANT stick at a time. Your app may fail to open the device until Garmin software is stopped.\n"
            << std::endl;
    }
    return !found;
}

#elif __linux__
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstdio>

static bool check_usb_is_available() {
    // Use lsusb to find ANT stick's bus and device numbers
    FILE* pipe = popen("lsusb | grep -i 0fcf", "r");
    if (!pipe) return false;

    char buffer[256];
    std::string dev_path;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Example: Bus 001 Device 007: ID 0fcf:1009 Dynastream Innovations, Inc. ANTUSB-m Stick
        int bus = 0, dev = 0;
        if (sscanf(buffer, "Bus %d Device %d:", &bus, &dev) == 2) {
            char path[64];
            snprintf(path, sizeof(path), "/dev/bus/usb/%03d/%03d", bus, dev);
            dev_path = path;
            break;
        }
    }
    pclose(pipe);

    if (dev_path.empty()) {
        std::cerr << "[WARNING]: ANT+ USB stick not detected by lsusb.\n";
        return false;
    }

    struct stat st;
    if (stat(dev_path.c_str(), &st) != 0) {
        std::cerr << "[WARNING]: Could not stat " << dev_path << "\n";
        return false;
    }

    // Check owner, group, and mode
    // World writable is mode & 002, group writable is mode & 020
    if ((st.st_mode & 0002) == 0 && (st.st_mode & 0020) == 0 && st.st_uid != getuid()) {
        std::cerr << "\033[1;31m[WARNING]\033[0m: You do not have write access to " << dev_path << "\n"
            << "This will prevent ANT+ applications from working.\n"
            << "→ To fix: add a udev rule for your stick's vendor/product id (see: /etc/udev/rules.d/99-antusb.rules)\n"
            << "and replug your stick.\n";
        return false;
    }
    // If you want, print confirmation:
    // std::cout << "[INFO]: ANT+ USB permissions look OK on " << dev_path << std::endl;
    return true;
}


#else
static bool is_garmin_process_running()
{
    return false;
}
#endif
