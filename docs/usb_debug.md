# Linux

List of steps to get ANT USB working on Linux. 

Use this command to list all connected USB devices:
```sh
lsusb
```

You should see something like this
```bash
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 001 Device 002: ID 0fcf:1009 Dynastream Innovations, Inc. ANTUSB-m Stick
```

## 1. **Check USB Permissions**
Most likely, the default user doesn't have permission to access `/dev/bus/usb/XXX/YYY` or `/dev/hidraw*` for this stick.
#### Quick test:
Plug in the stick. Find the device:
``` sh
ls -l /dev/bus/usb/001/002  # adjust to your bus/device numbers
ls -l /dev/hidraw*          # many ANT sticks also appear as hidraw
```
You should see ownership and permissions. If it’s something like `crw-rw---- root root`, your user cannot access it.

## 2. **Add udev Rule for ANT USB**
To allow user access, create the following udev rule (run as root):
``` sh
sudo nano /etc/udev/rules.d/99-antusb.rules
```
``` sh
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fcf", ATTRS{idProduct}=="1009", MODE="0666"
```
- `0fcf:1009` matches your "Dynastream Innovations, Inc. ANTUSB-m Stick" (seen in your `lsusb` output).

Reload udev and re-plug the stick:
``` sh
sudo udevadm control --reload
sudo udevadm trigger
# Or simply unplug/replug USB stick
```
Recheck the permissions on the device after replugging—your user should have access now.

## 3. **Check for Required Linux Packages**
Most SDKs depend on or sometimes `hidraw`. Make sure you have these: `libusb-1.0`
``` sh
sudo apt-get install libusb-1.0-0 libusb-1.0-0-dev
```
Sometimes, you also need:
``` sh
sudo apt-get install libudev-dev
```



