# Claude Instructions
## Introduction
This file contains tasks for Claude. Claude should only perform such a task after explicit request.

## Task 1
Eerder had je een mooi plan gemaakt:

Image-assets in flash zonder CMakeLists-wijzigingen

 Context

 Marius wil image-bestanden als flash-assets gebruiken in zijn apps. Zijn workflow:
 alle apps worden gestart door in main/main.cpp de juiste #include <App.ino> te
 uncommenten; de CMakeLists blijft daarbij altijd hetzelfde. EMBED_FILES zou per
 asset een CMakeLists-regel vergen én embedt álle assets van álle apps altijd —
 past niet bij die workflow.

 De oplossing: het patroon dat al bewezen is met main/flash_asset.h —
 een gegenereerde header met een const uint16_t-array. const → .rodata →
 memory-mapped uit flash via de cache (gemeten: 161 MB/s, 2–4% langzamer dan RAM,
 voorkopiëren zinloos). Voordelen boven EMBED_FILES:
 - Nul CMake-wijzigingen, nu en per toekomstige asset.
 - Alleen de assets die de actieve app #includet komen in de binary;
 assets van uitgecommentarieerde apps kosten niets.
 - Een asset reist mee met de app-directory (bijv.
 libs/CleanGUI_for_TFT_eSPI/examples/Inputfield/assets/); een quoted
 #include "assets/naam.h" in de .ino resolvet relatief aan het .ino-bestand,
 dus ook dat vergt geen include-dir-regels.

 Van dat plan wil ik nu graag de eerste stap uitgevoerd zien:
  1. Converter-tool: tools/img2rgb565.py

 Python-script (Pillow) dat elk door Pillow leesbaar beeld (PNG/JPG/BMP/GIF)
 omzet naar een header, in de stijl van main/flash_asset.h:

 python3 tools/img2rgb565.py speler.png            # -> speler.h naast de bron
 python3 tools/img2rgb565.py speler.png -o pad/x.h # expliciet doel
 python3 tools/img2rgb565.py speler.png --name heldSprite
 python3 tools/img2rgb565.py speler.png --transparent "#FF00FF"

 Gegenereerde header bevat:
 - #pragma once, #include <stdint.h>
 - #define <NAAM>_W / <NAAM>_H
 - bij --transparent: #define <NAAM>_TRANSP 0x.... (kleursleutel voor de
 transp-parameter van pushImage)
 - const uint16_t <naam>[W*H] = { ... }; — RGB565, zelfde waarde-conventie als
 flashAsset (gewone uint16-waarden; tft.setSwapBytes(true) bij direct naar
 TFT pushen, sprites regelen het zelf via pushImage)
 - commentaarkop met bronbestand + aanroep waarmee hij gegenereerd is
 (reproduceerbaarheid), en de gebruiks-snippet.

 Details:
 - RGB888→RGB565 met afronding; alpha-kanaal: pixels met alpha < 128 krijgen de
 transparantkleur (alleen zinvol met --transparent, anders waarschuwing).
 - 12 waarden per regel, zoals flash_asset.h.
 - Faalt netjes met melding als Pillow ontbreekt (pip install Pillow).

 Verder: ik wil graag dat de tools gebruik maken van een eigen virtual environment,
 middels een venv folder die subfolder is van de tools folder. 
 Verder wil ik in de tools folder graag een requirements.txt, waarmee de venv folder
 via een pip install geinitaliseerd kan worden. ik wil ook graag dat .claudeignore en .gitignore
 worden aangepast, zodat niet de contents van de venv folder en de python build in git terecht komt.
 