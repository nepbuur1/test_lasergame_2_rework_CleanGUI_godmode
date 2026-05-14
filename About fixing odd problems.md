## SHA_SIZE error
In test_lasergame_2\managed_components\espressif__esp_insights\src\esp_insights_cbor_encoder.c
// Marius: out of the blue kent hij SHA_SIZE niet. Ik heb het ter plekke maar even hardcoded.
#define SHA_SIZE 32

## LITTLEFS werkte niet
LITTLEFS werkte niet. heeft spifss partitie nodig.
Dus gebruik ik de volgende partitions.csv:
```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x170000,
```
.. en enable die in sdkconfig:
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

.. maar bovenstaande partitions.csv gebruikt 4MB RAM.
Bij het bouwen blijkt dat dat nog is ingesteld op 2MB RAM.
Dus ga naar Serial Flasher Config → Flash Size, en zet het op 4MB RAM.

De test in main.cpp werkt nu:
``#include <LITTLEFS_test.ino>``

## Touch reageert niet op ILI9488 (en met zelfde code wel op ILI9341)
Blijkt: 
ILI9488 kan SDO niet tri-state maken, en daardoor niet samen met TDO gebruiken.
Als je SDO loshaalt, werkt touch wel. Alleen kan de software dan geen informatie
van de display ophalen. Geen idee of dat nodig is.