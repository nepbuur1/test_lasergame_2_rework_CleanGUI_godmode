# Claude log

## 2026-07-21 — _bDrawable/isDrawable() toegevoegd aan CleanGUI-widgets

### Aanleiding
Sommige widgets moeten wél meedoen in de widget-tree en touch-afhandeling, maar
níets tekenen — typisch een onzichtbare TouchscreenButton die alleen een
touch-gebied afvangt.

### Wat is toegevoegd
- `IWidget` (`crt_IWidget.h`): `virtual bool isDrawable() = 0;`
- `Widget` (`internals/crt_Widget.h`): member `_bDrawable` (naast `_bVisible`/
  `_bEnabled`), gezet via nieuwe trailing ctor-parameter `bool bDrawable = true`,
  plus de `isDrawable()`-implementatie.
- `Panel`, `Text`, `TouchscreenButton`: ctor kreeg dezelfde trailing parameter
  (default `true`, dus geen enkele bestaande call site hoefde aangepast).
  TouchscreenButton geeft `bDrawable` door aan zowel de Panel-basis als het
  Text-kind (label van een onzichtbare knop wordt ook niet getekend).
- Guard `if (!isDrawable()) return;` vooraan in de functies die daadwerkelijk
  display-commando's queuen: `Panel::showPanel/hidePanel`, `Text::showText/hideText`.

### Semantiek
`show()/hide()` blijven gewoon werken voor niet-tekenbare widgets: `_bVisible`
en `_bEnabled` (en dus touch) worden normaal bijgehouden; alleen de
display-aanroepen worden overgeslagen. PageRoot/ButtonGroup/Keyboard/Inputfield
tekenen zelf niets en hadden geen aanpassing nodig.

### Verificatie
`idf.py build` compileert schoon (Inputfield-demo-app, gebruikt alle betrokken
widgettypes). Gedrag ongewijzigd voor bestaande code omdat de default `true` is.

## 2026-07-21 — Semantiek van Widget::show/hide in CleanGUI

### Aanleiding
Analyse van de flikker bij het typen in de Inputfield-demo. Om de fix goed te
kiezen moest eerst precies duidelijk zijn wat `show(true)`/`hide(true)` doen.

### Call-tree van een knop (tsButtonQ: Panel met afgeronde hoeken + Text-kind "q")
```
show(true)                          [Panel::show, crt_Panel.h:519]
├─ showPanel()                      -> display->fillSmoothRoundRect(...)  (queue)
└─ Widget::show(true)               [crt_Widget.h:275]
   ├─ _bVisible = true
   ├─ enable(false)                 -> bPressed=false, _bEnabled=true (touch AAN)
   └─ per kind: child->show(true)
      └─ text.showText()            -> display->drawString("q", ...)      (queue)

hide(true)                          [Panel::hide, crt_Panel.h:526]
├─ hidePanel()                      -> display->fillRect(rect, _colBg)    (queue)
└─ Widget::hide(true)
   ├─ _bVisible = false
   ├─ disable(false)                -> _bEnabled=false (touch UIT)
   └─ per kind: child->hide(true)
      └─ text.hideText()            -> LEGE STUB, tekent niets
```
De display-aanroepen zijn queue-writes naar AsyncDisplay; het echte tekenen
gebeurt later in de AsyncDisplay-taak (DisplayCommand::execute -> DisplayAdapter
-> _tft -> SPI -> ILI9341).

### Betekenis
- **`show(true)` = "schilder mij nú, en zet mij aan."** Achtergrond eerst, dan
  kinderen (painter's algorithm), plus `_bVisible=true` en touch inschakelen.
- **`hide(true)` = "schilder mij wég, en zet mij uit."** Verbergen bestaat niet
  op de ILI9341 - het is de eigen rect overschilderen met `_colBg`, plus
  `_bVisible=false` en touch uitschakelen.

### Drie asymmetrieën om te onthouden
1. **Tekst wordt nooit expliciet gewist.** `Text::hideText()` (crt_Text.h:847) is
   leeg; tekst verdwijnt alleen doordat de panel-fillRect eroverheen valt. Een
   Text zonder Panel eronder kan dus niet verborgen worden.
2. **Show/hide zijn imperatieve schilderopdrachten, geen state-wissels.** Er is
   geen renderloop die `_bVisible` leest; de recursie in Widget::show tekent
   kinderen onvoorwaardelijk. Daarom plakt een eenmalige `hide()` niet: de
   eerstvolgende `show(true)` op de ouder tekent het kind gewoon weer.
3. **Zichtbaarheid en touch zijn gescheiden velden.** Touch-afhandeling checkt
   alleen `_bEnabled` (crt_TouchscreenButton.h:176), niet `_bVisible`. "Niet
   tekenen maar wel touch ontvangen" is dus technisch mogelijk - relevant voor
   de onzichtbare touch-catchers (tsOutsideInputfieldButton/-KeyboardButton).

### Verwante bevinding (zelfde sessie)
De flikker bij typen komt doordat `Inputfield::updateButtonLabel` de hele
subtree hertekent, waarbij eerst `tsOutsideInputfieldButton` (100% vlak,
colFg==colBg==zwart) wordt geschilderd en daaroverheen de echte knop. Let op:
de vergelijkbare `tsOutsideKeyboardButton` is NIET puur onzichtbaar - die
schildert de paarse achtergrond van het keyboard en wist het vlak bij hide
(commentaar op crt_TouchscreenKeyboard.h:73: "displays the back of the
keyboard"). `bInvertedArea` is dus geen betrouwbaar signaal voor "niet tekenen";
een fix moet die twee gevallen uit elkaar houden. Nog niets gewijzigd.

---

## 2026-07-20 — Intern RAM versus PSRAM, en andere ESP-chips

### Aanleiding
Zijterzijde bij de sprite-metingen: het RAM op dit bord is de beperkende factor
(204 KB vrij, maar grootste aaneengesloten blok 110.592 bytes). Vraag: zijn er
ESP-chips met meer RAM?

### Intern SRAM per chip
| chip | intern SRAM |
|---|---|
| ESP32 (de CYD) | 520 KB |
| ESP32-S2 | 320 KB |
| ESP32-C3 | 400 KB |
| ESP32-S3 | 512 KB |
| ESP32-C6 | 512 KB |
| ESP32-P4 | 768 KB |

**Overstappen van chip levert dus vrijwel niets op.** Een paar honderd KB, waarvan
een flink deel naar stacks, WiFi/BT-buffers en fragmentatie gaat. De echte
oplossing is PSRAM.

### Het verschil tussen intern SRAM en PSRAM
**Intern SRAM** zit op de die zelf, direct aan de geheugenbus van de CPU. Toegang
kost effectief een paar klokcycli, er komt geen cache aan te pas, en het is
DMA-capable. Het is duur in siliciumoppervlak — vandaar dat het bij een paar
honderd KB blijft, ongeacht welke chip je pakt.

**PSRAM** is een aparte geheugenchip, in dezelfde package of ernaast, verbonden
via een SPI-bus (quad of octal). De CPU adresseert hem niet rechtstreeks: het
gaat via hetzelfde memory-mapped cache-mechanisme als flash. Een cache-hit is
snel, een cache-miss is een SPI-transactie. Dus: megabytes in plaats van
kilobytes, maar trager en afhankelijk van cachegedrag.

Vuistregel: intern SRAM is écht werkgeheugen; PSRAM gedraagt zich meer als een
zeer snelle, memory-mapped schijf.

Voor dit probleem telt vooral dat PSRAM als **één groot aaneengesloten blok**
beschikbaar komt — precies waar het hier op stukloopt.

### Praktische keuze
**ESP32-S3 met octal PSRAM** (8 MB). Octal is fors sneller dan de quad-PSRAM van
de klassieke ESP32, die de bus bovendien met flash deelt. TFT_eSPI heeft er al
een processor-header voor: `libs/TFT_eSPI/Processors/TFT_eSPI_ESP32_S3.h`.
`callocSprite` gebruikt automatisch `ps_calloc` zodra `CONFIG_SPIRAM_SUPPORT`
aan staat en er PSRAM gevonden wordt (`Extensions/Sprite.cpp:165`).

### Twee kanttekeningen
- **PSRAM en DMA gaan niet samen** in TFT_eSPI. `Sprite.cpp:165` valt bewust terug
  op intern RAM zodra `_tft->DMA_Enabled` waar is. Logisch: de DMA-engine leest
  alleen internal DRAM (zie de DMA-uitleg in de entry hieronder).
- **De CYD is een ESP32-WROOM-32 zonder PSRAM.** Dit betekent dus een ánder bord,
  geen configuratie-instelling.

---

## 2026-07-20 — Sprites, flash-assets en de DMA-omslag (CYD / ILI9341)

### Aanleiding
Vervolg op de DMA-sessie hieronder. Vraag: hoeveel levert DMA op als de
renderloop het hele beeld opnieuw tekent, en wat is de rol van sprites bij
images die in flash staan?

### 0. Wat DMA hier precies doet
Zonder DMA **ís de CPU de datapomp**: `pushPixels()` schrijft 16 woorden (64 bytes)
in de SPI-FIFO-registers `SPI_W0..W15`, wacht tot `SPI_USR` klaar is, en herhaalt.
Fullscreen = 2400 van die rondjes. De gaatjes tussen de bijvullingen zijn precies
waarom we 36 MHz effectief maten in plaats van 40.

Met DMA krijgt een state machine op de geheugenbus één descriptor (adres + lengte)
en vult die de FIFO zelfstandig bij. De CPU zet het op en is vrij. Twee
hardware-eisen volgen daaruit: de engine kan **alleen internal DRAM** lezen (niet
door de flash-cache heen — vandaar dat flash-assets eerst naar een sprite moeten),
en de buffer moet **onaangeroerd blijven** tot de transfer klaar is — dát is de
echte reden voor double buffering, geen software-conventie.

Softwarematig gebruikt TFT_eSPI de peripheral via twee paden tegelijk: normaal
direct registers rammen, en daarnaast de IDF `spi_master`-driver die `initDMA()`
opzet (`spi_bus_initialize` + `spi_bus_add_device` → `dmaHAL`). `pushImageDMA()`
doet dan: `dmaWait()` → `setAddrWindow()` via de registerroute (commando's zijn te
klein voor DMA) → `spi_device_queue_trans()`, dat meteen terugkeert. `queue_size`
is 1, dus er loopt er hooguit één tegelijk.

Daaruit volgt alles wat we maten: +10% ruwe doorvoer (geen gaatjes meer), ~40 µs
vaste overhead per push (waardoor DMA onder 32x32 juist verliest), en een vrij
CPU-venster ter grootte van de transfer (waardoor de DMA-kolommen vlak blijven).
De engine is een domme kopieermachine die niets kan omdraaien — sprites slaan
16bpp al byte-swapped op, dus `setSwapBytes(false)` is verplicht.

### 1. Fullscreen redraw is transfer-bound op ~32 fps
Fullscreen in stroken van 80px, bij oplopende tekenbelasting:
```
tekenwerk    zonder DMA          met DMA            winst
0,86 ms      35,20 ms (28,4)     30,98 ms (32,3)    1,14x
2,10 ms      36,43 ms (27,4)     30,99 ms (32,3)    1,18x
4,06 ms      38,38 ms (26,1)     31,01 ms (32,2)    1,24x
7,88 ms      42,21 ms (23,7)     31,06 ms (32,2)    1,36x
```
De DMA-kolom is **vlak**: 30,98 / 30,99 / 31,01 / 31,06. Tekenwerk vertienvoudigt,
frametijd stijgt 0,08 ms. Volledig verstopt achter de transfer.

153.600 bytes bij 4,98 MB/s = 30,8 ms. Dat is 32 fps en dat is een **hard
plafond** — geen enkele optimalisatie komt daaroverheen. DMA zet je erop en houdt
je erop. Theoretisch maximum is 2x, precies wanneer tekenen even lang duurt als
de transfer. Daarboven word je render-bound.

### 2. Twee modellen, en alleen één ervan heeft double buffering nodig
Dit onderscheid bleek de kern van de verwarring:

- **Immediate mode** (elk frame pixels *genereren* in een buffer): double
  buffering verplicht, anders schrijf je in wat de DMA-engine uitleest.
  Kost een tweede buffer. Zo werkt SpriteRotatingCube.
- **Statische sprites** (assets één keer bouwen, daarna alleen *pushen*):
  niemand schrijft in de sprite, dus **geen tweede buffer, nul extra bytes**.
  De overlap is dan met gamelogica in plaats van met tekenwerk.

Let op: het patroon "asset uit flash naar sprite blitten" is een *schrijf*
actie, dus dat valt onder immediate mode en heeft wél double buffering nodig.

### 3. Bij kleine sprites is DMA verlies
40 sprites per frame, statische assets:
```
sprite   zonder DMA   met DMA    100us logica/entity: zonder → met
16x16    4,86 ms      6,39 ms    8,89 → 6,13 ms  (1,45x)
32x32   18,57 ms     18,67 ms   22,56 → 18,67 ms (1,21x)
64x64   73,22 ms     67,84 ms   77,25 → 67,83 ms (1,14x)
```
Vaste kosten van `spi_device_queue_trans` + `dmaWait` zijn ~40 µs per push. Bij
512 bytes (16x16) is dat meer dan je aan de transfer wint. **Omslagpunt ligt rond
32x32.**

Met gamelogica erbij: de DMA-kolom blijft weer vlak (6,39→6,13, 18,67→18,67,
67,84→67,83). 4 ms logica per frame volledig geabsorbeerd.

### 4. Flash-assets: de kopie is niet je probleem
```
sprite   uit flash              uit RAM      verschil
16x16     7,5 us                 7,2 us       4%
32x32    17,7 us                17,4 us       2%
64x64    50,9 us (161 MB/s)     49,9 us       2%
```
De flash-cache doet zijn werk. **Assets naar RAM kopiëren om sneller te blitten
heeft geen zin.** En 64x64 blitten kost 51 µs tegen 1645 µs om het naar het
scherm te sturen — de kopie is **3%** van de transfer.

### 5. Vermijd per-pixel werk, niet sprites
```
spr.drawPixel()             581 ns/pixel
rauwe store via getPointer  121 ns/pixel   (4,8x sneller)
spr.pushImage() (bulk)     12,4 ns/pixel   (47x sneller dan drawPixel)
```
`drawPixel` doet per pixel een `_created`-check, `_vpOoB`-check, datum-optelling,
vier range-vergelijkingen, een bpp-branchketen en een byte-swap rond één store.

Maar `pushImage` is 10x sneller dan een handgeschreven pixelloop, want het
kopieert rijgewijs in bulk. De les is dus **niet** "vermijd sprites" maar
"vermijd per-pixel werk".

Volledige pipeline (blit uit flash + push, 40 assets/frame) wijkt nauwelijks af
van alleen pushen: 76,09 vs 73,22 ms bij 64x64. Blitten is 4% van het frame.

### Wat een sprite toevoegt t.o.v. boing_ball's scanline-buffers
Een sprite ís een RAM-buffer plus teken-API; `getPointer()` geeft dezelfde
`uint16_t*`. Op de transfer: nul verschil, DMA ziet alleen een pointer. Het
verschil is **geheugenstrategie**: boing_ball streamt 320x240 met 1,2 KB
(twee scanlines), een sprite zou 153 KB kosten. Je betaalt RAM voor willekeurige
toegang, compositie en de teken-primitieven.

### Praktische vuistregels
- Sprites ≥32x32 én noemenswaardige CPU-tijd per frame → DMA loont.
- Veel kleine sprites, weinig logica → DMA overslaan.
- Fullscreen redraw → je zit vast aan 32 fps, ongeacht wat je doet.
- Blit met `pushImage`, nooit met een `drawPixel`-loop.

### Bestanden
`main/spi_benchmark.h` uitgebreid met `benchFullScreen()`, `benchRenderOnly()`,
`benchGameSprites()`, `benchBlit()`, `benchPerPixel()`, `benchAssetPipeline()`.
Nieuw: `main/flash_asset.h` (gegenereerde 64x64 RGB565 asset in `.rodata`).

`main.cpp` draait nu SpriteRotatingCube (**151 fps**; Bodmer noteert voor ESP32
40 MHz: 127 fps zonder DMA, 145 met — wij zitten hoger, vermoedelijk door de
HSPI IOMUX-pinnen). Twee dingen waren nodig om dat voorbeeld te bouwen:
- De map heet `DMA test` **mét spatie**. Die kan niet in
  `COMPONENT_ADD_INCLUDEDIRS` — de legacy `register_component()` splitst de lijst
  alsnog op spaties. Opgelost met een relatief pad in de `#include`.
- `drawCube()` forward declaren; de `.ino` roept hem aan vóór de definitie.

`Flash_Jpg_DMA` is **niet** gedraaid: vereist TJpg_Decoder, dat niet in de repo
zit. Bovendien gebruikt dat voorbeeld helemaal geen sprites — het decodeert JPEG
uit flash naar 16x16 tegels in kale arrays. `Bouncy_Circles` past hier sowieso
niet: twee sprites van 240x160 = 153.600 bytes tegen een vrij blok van 110.592.

---

## 2026-07-20 — DMA gemeten (CYD / ILI9341)

### Aanleiding
Vervolg op de SPI-sessie hieronder: daar stond DMA nog als "grootste resterende
snelheidswinst" genoteerd. Nu daadwerkelijk aangezet en gemeten.

### Wat er nodig was
Niets aan de configuratie. `ESP32_DMA` is in `Processors/TFT_eSPI_ESP32.h`
al gedefinieerd voor elke niet-parallelle bus; de DMA-code was dus altijd al
meegecompileerd. Ontbrak alleen de runtime-aanroep `tft.initDMA()` plus
`pushImageDMA()` in plaats van `pushSprite()`.

`initDMA()` doet zelf `spi_bus_initialize()` op dezelfde HSPI-host die Arduino's
SPI-laag al gebruikt. Dat botst in de praktijk niet — geen fout, geen assert.

Let op: `pushImageDMA()` slikt de sprite-pointer rauw. Sprite-pixels staan al in
de byte-volgorde van het display, dus `tft.setSwapBytes(false)` vóór het pushen.

### Meetresultaten (40 MHz, HSPI, `main/spi_benchmark.h`)
```
--- blocking pushSprite ---
240x 30 :   3.200 ms/push,  4.50 MB/s,  36.0 MHz effectief
240x160 :  17.007 ms/push,  4.52 MB/s,  36.1 MHz effectief
--- pushImageDMA ---
240x 30 :   2.939 ms/push,  4.90 MB/s,  39.2 MHz effectief
240x160 :  15.425 ms/push,  4.98 MB/s,  39.8 MHz effectief

--- render+push, 240x80 ---
zonder DMA (1 sprite) :  8.947 ms/frame, 111.8 fps
met DMA (2 sprites)   :  7.765 ms/frame, 128.8 fps
```

### Conclusie
**Ruwe doorvoer: +10%.** Van 36 naar ~40 MHz effectief. Dat is precies wat je
kunt verwachten: DMA haalt de per-pixel CPU-overhead uit de transfer, waardoor
de bus vrijwel de volle 40 MHz haalt. Meer zit er niet in — 40 MHz ís het
plafond, en blocking pushSprite zat daar al op 90%.

**Render+push double buffered: +15% (112 → 129 fps).** De rekensom laat zien
waarom niet meer: een 240x80 sprite kost 38.400 bytes, bij 4,98 MB/s is dat
7,71 ms. Gemeten werd 7,765 ms/frame. Het tekenwerk is dus **volledig** achter
de transfer weggewerkt — de overlap is perfect, er valt niets meer te winnen.

De winst van double buffering schaalt dus met hoeveel je per frame tekent, tot
maximaal de transfertijd. Bij dit lichte tekenwerk (fillSprite + 8 cirkels,
~1,2 ms) is dat weinig. Bij zwaarder tekenwerk loopt het op naar bijna 2x, maar
nooit verder: zodra het tekenen langer duurt dan de transfer is de CPU weer de
bottleneck.

### Kosten
Double buffering kost een tweede sprite. Grootste aaneengesloten blok is 110.592
bytes, dus twee 16bpp-sprites samen mogen niet boven ~110 KB uitkomen: 240x80
past (2x 38,4 KB), 240x160 niet (2x 76,8 KB). Fullscreen 240x320 past sowieso
niet, ook niet enkel — dat blijft 8bpp-met-palette of stroken.

### Status
`main/spi_benchmark.h` bevat nu ook `benchOneSizeDma()` en
`benchDoubleBuffer()`. De aanroep in `main.cpp` staat weer uitgecommentarieerd.
De app zelf is **niet** omgebouwd naar DMA — dat is een aparte ingreep.

---

## 2026-07-20 — SPI-instellingen display (CYD / ILI9341)

### Aanleiding
Vraag over beschikbaar RAM voor sprites, uitmondend in een analyse van de
display-interface. Daarbij bleek de SPI-configuratie suboptimaal.

### Context: geheugen (gemeten, niet geschat)
Target is een **plain ESP32 zonder PSRAM** (`CONFIG_SPIRAM is not set`).

Op het bord gemeten na volledige init:
```
free heap 204144, largest block 110592
```

**Die twee getallen zijn niet inwisselbaar en dat is hier het hele punt.** Er is
204 KB vrij, maar het grootste aaneengesloten blok is 110.592 bytes. Een sprite
vereist één aaneengesloten allocatie, dus het tweede getal is het plafond.

| sprite | bytes | past? |
|---|---|---|
| fullscreen 16 bpp | 153.602 | **nee** — bevestigd, `createSprite` faalt |
| fullscreen 8 bpp + palette | 76.802 | ja |
| fullscreen 4 bpp | 38.402 | ja |
| 240x230 16 bpp (praktisch max) | 110.402 | net |
| band 240x30 16 bpp | 14.402 | ruim |

Merk op: in TFT_eSPI *is* een sprite de framebuffer — er is geen apart
framebuffer-mechanisme. Direct naar de TFT tekenen kost 0 RAM. De ILI9341 heeft
172.800 bytes GRAM aan boord, maar dat is precies één frame en fungeert als
frontbuffer (continu uitgescand op ~70 Hz); het is geen bruikbaar scratchgeheugen.

### Wijzigingen in `libs/TFT_eSPI/User_Setup.h`

**1. `SPI_FREQUENCY`: 27 MHz → 40 MHz**

De ESP32 deelt de 80 MHz APB-klok met een geheel getal. 27 MHz bestaat niet als
deler en leverde in de praktijk 80/3 = 26,67 MHz. 40 MHz is exact 80/2.
Effectief ~50% meer bandbreedte.

**2. `USE_HSPI_PORT` ingeschakeld**

Dit is *geen* pinwijziging — de pinconfiguratie (MISO 12, MOSI 13, SCLK 14,
CS 15) blijft ongemoeid. Het bepaalt welke SPI-controller die pinnen aanstuurt.

Op dit bord (CYD, ESP32-2432S028R) zijn 12/13/14/15 de IOMUX-pinnen van HSPI;
de bedrading is dus voor HSPI ontworpen. Zonder deze define draaide de display
op VSPI, waardoor alle signalen door de GPIO-matrix werden gerouteerd in plaats
van rechtstreeks. Dat werkt, maar kost latency en beperkt de haalbare klok.

De expliciete pin-defines in User_Setup.h overrulen sowieso de fallback-blokken
in `Processors/TFT_eSPI_ESP32.h:289-320` — dat zijn `#ifndef`-guards.

### Meetresultaten

Beide configuraties gebouwd, geflasht en gemeten op het bord met dezelfde
benchmark (`main/spi_benchmark.h`, N herhalingen per formaat,
`esp_timer_get_time()` eromheen).

| sprite | 27 MHz / VSPI | 40 MHz / HSPI | winst |
|---|---|---|---|
| 240x30 (banner) | 4,651 ms | **3,200 ms** | 1,45x |
| 240x160 (half scherm) | 24,733 ms | **17,009 ms** | 1,45x |
| doorvoer | 3,10 MB/s (24,8 MHz eff.) | **4,52 MB/s (36,1 MHz eff.)** | +46% |

**45% sneller, consistent over beide formaten.** Bij een half scherm scheelt dat
~8 ms per frame: het verschil tussen 40 en 59 fps.

De effectieve klok ligt ~10% onder de nominale door `setWindow`-overhead per push
(CASET/PASET/RAMWR dragen geen pixels). Die overhead is in beide configuraties
gelijk, dus de verhouding klopt.

### Status
- **40 MHz is stabiel.** Meermaals geflasht en geboot, geen crashes, schone
  boot-log, geen ruis.
- **Touch werkt na de poortwissel.** Kalibratie wordt geladen en gevalideerd
  (`Rotation MATCHES - calibration data is valid`), touch-events komen door.
- Bord draait nu op 40 MHz / HSPI, geverifieerd via serial na de laatste flash.
- Benchmark staat uitgecommentarieerd in `main/main.cpp` (include + aanroep),
  `main/spi_benchmark.h` blijft staan als referentiepunt.

### Correctie op een eerdere bewering
Tijdens deze sessie beweerde ik tussentijds dat een fullscreen 16-bpp sprite
"mogelijk wel past", op basis van de 167 KiB DRAM-regio uit `heap_init`. Dat was
onjuist. Alloceerbaar is het **grootste aaneengesloten blok** (110.592 bytes),
niet de vrije heap (204.144). De oorspronkelijke conclusie "past niet" was juist.

### Nog openstaand
- **80 MHz** te proberen. Buiten ILI9341-spec (datasheet noemt ~10 MHz voor
  schrijven), maar op een CYD met korte sporen vaak haalbaar. Falen is zichtbaar
  en onschuldig: ruis, verkeerde kleuren of bevroren beeld. Terugvallen kan door
  in User_Setup.h weer de 27 MHz-regel te activeren.
- **DMA staat uit** (`DMA_Enabled = false`, geen `initDMA()`-aanroep in het
  project). Met `initDMA()` + `pushImageDMA()` kan de CPU de volgende frame
  opbouwen tijdens de transfer van de vorige — daar is `createSprite(w, h, 2)`
  voor bedoeld. Grootste resterende winst na de kloksnelheid; de benchmark ligt
  klaar om het effect te meten.

