# vzekeyprovider running on low-power Arduino ESP32 board

Using PlatformIO as a development platform for the ESP32 board, this project
aims at a small webserver in a local WiFi network. Once booted, it acts
as an access point in which SSID and Password of the WiFi home network need
to be entered, together with the base64-encoded encrypted zfs key.

Then, the ESP32 connects to the WiFi home network and listens to /password
requests of a dedicated peer IP address and returns the encrypted zfs key.

All sensitive data such as WiFi Password is not stored in the ESP devices
file system but only kept in memory.
