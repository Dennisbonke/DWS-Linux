# DWS-Linux
DWS System Tray application for Linux. This program fetches the current alert code as indicated by the [Defcon Warning System](https://defconwarningsystem.com) and shows it in the tray bar. It will also give a notification if the code changes.
By default, it will refetch the code every hour.

# Building
To build, make sure you've installed the development packages of `libayatana-appindicator`, `libcurl` and `libnotify`.
Then, issue the following commands:
```
mkdir build
cd build
cmake ..
make
```

To install, `sudo make install`.

For GNOME users, be sure to install and enable the Ubuntu AppIndicators extension, else the tray part will not work.

# TODO List
- Add sounds to the notifications.
- Make the sounds optional.
- Make the notifications optional.
