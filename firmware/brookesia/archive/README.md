# Archived firmware components

This directory stores implementation snapshots that are intentionally excluded
from the ESP-IDF component discovery path.

components/AIChats contains the former standalone Xiaozhi protocol
implementation. It is retained only as a migration reference while the active
components/XiaozhiApp application uses the managed espressif/esp_xiaozhi
component.

Do not add this directory to EXTRA_COMPONENT_DIRS. Production changes belong
in the active component tree.
