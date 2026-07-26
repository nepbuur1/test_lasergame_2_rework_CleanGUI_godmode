# Project notes

## Hardware
- **Board:** CYD (ESP32-2432S028R) — geïntegreerde ESP32 + ILI9341 240x320 TFT + XPT2046 touch.
- **Target:** plain ESP32, **geen PSRAM** (`CONFIG_SPIRAM` staat uit). Alle sprites
  moeten uit de ~110-140 KB vrije interne DRAM komen.
- **Pinnen liggen vast in het bord** — niet configureerbaar. TFT op MISO 12 /
  MOSI 13 / SCLK 14 / CS 15 / DC 2 (HSPI IOMUX-pinnen), touch op 32/33/25/39.
- **Seriële poort:** `/dev/ttyUSB0`.

## Logboek
Verslagen van sessies gaan in **`devdocs/Claude_log/Claude_log.md`**.
Nieuwste entry bovenaan onder een `## <datum> — <onderwerp>` kop.
Niet in de repo-root aanmaken.

## Bouwen en flashen
```
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

## Display / TFT_eSPI
- Config staat in `libs/TFT_eSPI/User_Setup.h` (geselecteerd via `User_Setup_Select.h`).
- SPI draait op 40 MHz via de **HSPI**-poort (`USE_HSPI_PORT`). De ESP32 deelt de
  80 MHz APB-klok met een geheel getal, dus kies alleen echte delers: 40 (80/2),
  26,67 (80/3), 20 (80/4). Een waarde als 27 MHz levert stilzwijgend 26,67 MHz op.
- Een **sprite ís de framebuffer** — TFT_eSPI heeft geen apart framebuffer-mechanisme.
  Direct naar de TFT tekenen kost 0 RAM; een sprite kost `2*w*h + 2` bytes bij 16 bpp.
  Fullscreen 16 bpp (153.602 bytes) past niet; 8 bpp met palette (76.802) wel.
- De ILI9341 heeft 172.800 bytes eigen GRAM, maar dat is de **frontbuffer** die
  continu op ~70 Hz wordt uitgescand — geen bruikbaar scratchgeheugen, geen tweede
  pagina voor hardware double buffering.
- DMA is uitgebreid gemeten (zie logboek). Vuistregels:
  - Ruwe doorvoer +10% (36 → 40 MHz effectief). 40 MHz is het busplafond.
  - **Fullscreen redraw zit vast op ~32 fps** (153.600 bytes / 4,98 MB/s = 30,8 ms),
    ongeacht optimalisaties. DMA zet je op dat plafond en houdt je erop.
  - **Omslagpunt ligt rond 32x32.** Daaronder is DMA *langzamer* — de vaste
    ~40 µs per `queue_trans` weegt niet op tegen de winst.
  - DMA loont pas als er ook CPU-werk per frame is om mee te overlappen.
- Double buffering is nodig zodra je elk frame **in** een buffer schrijft
  (renderen of blitten). Sprites die alleen worden *gepusht* hebben het niet
  nodig: nul extra bytes.
- Blitten uit flash is goedkoop (161 MB/s, ~3% van de transfertijd) en de
  flash-cache maakt het net zo snel als uit RAM. Assets voorkopiëren heeft geen zin.
- **Twee heel verschillende snelheden — haal ze niet door elkaar:**
  - *Naar het scherm* (SPI): **~400 ns/pixel**, en dat is een hard plafond.
    2 bytes bij 40 MHz. Gemeten: fullscreen clear 76.800 px in 31,4 ms =
    409 ns/pixel; 64x64 pushen kost 1645 µs = 402 ns/pixel.
  - *In geheugen* (sprite): `spr.pushImage()` 12,4 ns/pixel, `spr.drawPixel()`
    581 ns/pixel — factor 47. Gebruik daar altijd bulk-operaties.

  De sprite-getallen zijn **geen** display-doorvoer: 12,4 ns/pixel is 161 MB/s
  en over een 40 MHz-bus fysiek onmogelijk. Reken tekenwerk naar het scherm dus
  altijd met ~400 ns/pixel.
- Praktisch gevolg: bijna al het tekenwerk is **busgelimiteerd**, niet
  CPU-gelimiteerd. Gemeten met het Panel-voorbeeld zit zelfs
  `fillSmoothRoundRect` maar 4% boven het busplafond, en levert een release-build
  (-O2) op die posten 0% op. Zoek winst dus in *minder pixels versturen*, niet in
  snellere code. De grootste hefboom is meestal: niet elk frame de hele
  achtergrond wissen als je hem toch overtekent.
- Uitzondering: een **transparante** `pushImage` betaalt ~12 µs per *run* van
  aaneengesloten niet-transparante pixels (één `setWindow` per run). Goedkoop bij
  lange runs, duur bij veel losse eilandjes — het aantal runs telt, niet het
  aantal pixels. Gemeten: een 64x64 ring met 1.592 zichtbare pixels in 200 runs
  kostte 3,06 ms waar de bus 0,65 ms nodig had (factor 4,7).
- De app gebruikt nog gewoon `pushSprite()`; niets is omgebouwd naar DMA.
