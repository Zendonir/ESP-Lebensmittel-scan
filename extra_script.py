Import("env")
import os

# GFX Library 1.5+ includes esp32-hal-periman.h from the Arduino-ESP32 core.
# PlatformIO does not automatically expose this header when compiling libraries,
# so we add the core path here.
framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    core_path = os.path.join(framework_dir, "cores", "esp32")
    if os.path.isdir(core_path):
        env.Append(CPPPATH=[core_path])
