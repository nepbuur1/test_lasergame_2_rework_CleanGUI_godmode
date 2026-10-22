# The Arduino IDE component (arduino-esp32)

There is no need to clone arduino-esp32 into this directory anymore.

The ESP-IDF Component Manager fetches it automatically. The dependency is declared in
`main/idf_component.yml`:

    espressif/arduino-esp32: ^3.3.5

On the first `idf.py build` (or `idf.py reconfigure`), the Component Manager downloads it,
together with its own dependencies, into `managed_components/espressif__arduino-esp32`.
The exact versions it chose are recorded in `dependencies.lock`. Both `managed_components/`
and `dependencies.lock` are in `.gitignore`.

To use a different arduino-esp32 version, change the version constraint in
`main/idf_component.yml` and run `idf.py reconfigure`.

The Arduino examples, for instance `libraries/WiFi/examples/WiFiScan`, are now under
`managed_components/espressif__arduino-esp32/libraries/`.

This directory itself only holds components that are kept in the repository as a local copy,
such as `espressif__esp_insights`.
