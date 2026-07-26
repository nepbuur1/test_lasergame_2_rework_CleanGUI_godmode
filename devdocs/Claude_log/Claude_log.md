# Claude log

## 2026-07-26 — getScreenSize klopte niet, en de layout leunde op een race

### Wat er mis was
`crt_TFT_eSPI_DisplayAdapter::getScreenSize()` wisselde breedte en hoogte om bij
rotatie 1 en 3. Maar TFT_eSPI's `width()`/`height()` zijn na `setRotation()` zelf
al gedraaid, dus dat was een omwisseling te veel. Gemeten tijdens het schuiven van
het toetsenbord:

```
getScreenSize=(240,320)   root=(320,240)
```

Precies verwisseld, en daardoor knipte de nieuwe regio-clipping een kwart van de
schermbreedte weg.

### Waarom het jarenlang goed leek te gaan
De adapter kende zijn rotatie pas in `begin()` of `touchCalibrate()`. In de
demo's met `AsyncDisplay` draait `touchCalibrate()` in de AsyncDisplay-taak,
terwijl `PageRoot` de schermmaat opvraagt vanuit de andere taak - en beide taken
worden na hun `vTaskDelay(1000)` op hetzelfde moment wakker. `PageRoot` trof
daardoor het venster waarin `_rotation` al 3 was maar `_tft.setRotation()` nog
niet was uitgevoerd. In dat venster meldt `_tft` nog portret, en dan levert de
dubbele omwisseling toevallig het juiste getal op. **De schermafmeting berustte
dus op een race die telkens dezelfde kant op viel.**

### De reparatie
- `setRotation()` toegevoegd aan `IDisplaySynchrone`, waar `getScreenSize()` ook
  woont. `AsyncDisplay` geeft de rotatie door in zijn **constructor**, dus voordat
  er een taak draait. De adapter onthoudt hem en past hem toe zodra de TFT
  daadwerkelijk is gestart (`_bBegun`).
- `getScreenSize()` leidt de maat af van `TFT_WIDTH`/`TFT_HEIGHT` (in portret
  gedefinieerd) plus de rotatie, en niet meer van `_tft.width()/height()`. Het
  antwoord is daarmee hetzelfde, ongeacht wanneer je het vraagt.

Na de fix: `getScreenSize=(320,240)` en `root=(320,240)`.

### Waarom dit geen enkele demo omgooit
De nieuwe waarde is precies de waarde die de demo's via de gewonnen race al
kregen. Was de race andersom uitgevallen, dan hadden ze (240,320) gekregen en er
zichtbaar verkeerd uitgezien. De fix maakt het deterministisch, niet anders.

`Widget::clipToRoot()` blijft op de rect van de root clippen: de layout is op de
root gebaseerd, dus dat is per definitie consistent - ook als de root ooit kleiner
dan het scherm zou zijn.

## 2026-07-26 — Widgets verplaatsen, en een queue die stilzwijgend werk weggooide

### Vraag
Zou een toetsenbord dat van onderaf in beeld schuift werken door herhaald de
y-positie aan te passen, en zet de widget de vrijgekomen ruimte terug?

**Nee.** `Widget::setLocPos()` herberekent alleen `_locPosPix` en geeft de nieuwe
globale positie door aan de kinderen; het stuurt nul tekencommando's. Tekenen
gebeurt uitsluitend in `show()`/`hide()`, en `hidePanel()` vult de rect waar het
paneel op dat moment staat met zijn eigen `_colBg`. Het bestaande voorbeeld
`crt_TestAsyncDisplay.h:105-115` doet daarom bewust hide -> setLocPos -> show.

### Wat er is gebouwd
- **`setClipRect`/`clearClipRect` in `IDisplayDraw`**, via de viewport, en door de
  commandoqueue heen zodat de clip in de juiste volgorde geldt.
- **`Widget::redrawRegion()`** loopt door de tree en tekent alleen wat de regio
  snijdt. `paint()` is nieuw in `IWidget`: "teken alleen mijzelf", zonder aan
  `_bVisible`/`_bEnabled` te komen.
- **`Widget::moveTo()`** verplaatst, berekent welke stroken vrijkomen (maximaal
  vier; bij een rechte schuif is er een niet-leeg) en laat de tree die herstellen
  vanaf de root. Bewust geen hide/show: dat wist eerst de hele widget (flikker),
  vult met een gokkleur in plaats van te herstellen, en zou de touch-afhandeling
  elke stap aan en uit zetten.
- **`IWidget` kreeg `getParent()`/`setParent()`** - die verwijzing bestond niet,
  dus een widget kon zijn ouder niets vragen.
- Het viewport-conflict opgelost: `drawImage` en `drawStringOpaque` zetten sinds
  eerder vandaag zelf een viewport en deden `resetViewport()`. Met een actieve
  clip gooiden ze die weg. Ze snijden nu door met de clip via
  `applyViewport()`/`restoreViewport()`.

### De "crash" was stilzwijgend commandoverlies
Gemeld: het schuiven liep vast midden in de tweede toetsenrij. Reproduceerbaar
gemaakt door de schuif via dezelfde vlag te triggeren die een aanraking zet.
Resultaat: **682 weggegooide tekencommando's**, types 7 (drawString), 17
(fillBeveledRect) en 19 (**clearClipRect**).

`crt_Queue.h:26` maakt de queue met `bWriteWaitIfQueueFull=false`, dus
`xQueueSend` krijgt timeout 0 en geeft bij een volle queue simpelweg `false`
terug - `AsyncDisplay` negeerde die returnwaarde. Sneuvelt een `clearClipRect`,
dan blijft de clip staan en verandert er daarna niets meer op het scherm. Het
programma draait door; het beeld staat stil. Dat ziet eruit als een crash.

Opgelost: `AsyncDisplay::writeCommand()` wacht tot er plaats is (queue nu met
`bWriteWaitIfQueueFull=true`). Wordt er geschreven vanuit de tekentaak zelf -
touch-afhandeling draait daar - dan zou blokkeren een impasse met zichzelf zijn,
dus maakt hij eerst plaats door zelf een commando uit te voeren. Plus een teller
`getNofQueueFullEvents()`, zodat queuedruk zichtbaar is in plaats van stil.

### Twee eigen misslagen, voor de volledigheid
1. **"Het hangt"** - te snel geconcludeerd uit stilgevallen logregels. Er logde
   daarna simpelweg niets; die stilte bewees niets.
2. **Een optimalisatie die averechts werkte.** Eerder vandaag bleek release 9x
   sneller (284 -> 31 ms/stap) en wees ik `dumpStackHighWaterMarkIfIncreased()`
   in de AsyncDisplay-lus aan als oorzaak. Die aanroep verplaatst naar de timer
   in plaats van per commando: **767 ms/stap**, bijna drie keer trager.
   Teruggedraaid. De werkelijke oorzaak van de wisselende debug-tijden was mijn
   eigen spinlus `while (!write) vTaskDelay(1)` tegen een taak van gelijke
   prioriteit - die gaf 284, 767 en >25000 ms bij identieke code. Met een echt
   blokkerende queue is het stabiel. **Les: laat de queue wachten, ga niet zelf
   pollen.**

### Metingen (15 stappen van 8 pixels, toetsenbord 320x120)
| | debug | release |
|---|---|---|
| per stap | 183 ms | **21 ms** |
| hele schuif | 2757 ms | **319 ms** |

Beter dan de ~38 ms/stap uit het plan, doordat toetsen buiten beeld worden
weggeculld. Zonder buffer blijft een volledige hertekening per stap nodig: de
verschoven inhoud kan niet worden geblit.

### Ontwerppunt: de root tekent niets
`PageRoot` heeft geen eigen kleur, dus zonder een widget die de achtergrond
bezit, is er niets om een vrijgekomen strook mee te herstellen en blijft het
oude beeld staan. Het Inputfield-voorbeeld heeft daarom een `backgroundPanel`
gekregen, als eerste kind toegevoegd en dus als eerste getekend.

### Openstaande bug: getScreenSize klopt niet, en de layout leunt op een race
Gemeten tijdens het schuiven:
```
getScreenSize=(240,320)   root=(320,240)
```
Precies verwisseld. `crt_TFT_eSPI_DisplayAdapter.h:86` wisselt breedte en hoogte
om bij rotatie 1 en 3, terwijl TFT_eSPI's `width()`/`height()` na `setRotation()`
zelf al gedraaid zijn - een dubbele omwisseling.

Dat het tot nu toe goed ging: `PageRoot` vraagt zijn maat op terwijl
`AsyncDisplay` gelijktijdig `touchCalibrate()` draait (beide taken worden na hun
`vTaskDelay(1000)` tegelijk wakker). `PageRoot` treft daardoor het venster waarin
`_rotation` al 3 is maar `_tft.setRotation()` nog niet is uitgevoerd, en dan
levert de dubbele omwisseling toevallig het goede getal op. **De schermafmeting
van dit project berust dus op een race.**

`Widget::clipToRoot()` clipt daarom op de rect van de root en niet op
`getScreenSize()`: de layout is op de root gebaseerd, dus dat is per definitie
consistent. De adapterbug zelf staat nog open - de swap eruit halen zou zonder
meer elke demo op zijn kant zetten, want dan krijgt PageRoot in dat venster
240x320.

## 2026-07-26 — Toetsenbord in toetsstijl, en een verzonken invoerveld

### Aanleiding
Het keyboard had rode afgeronde toetsen op een paarse rug. Gevraagd: de stijl van
een referentie-afbeelding - grijze rechte toetsen met een glanslijntje boven en
links, en donker daarbuiten als schaduw van de kier. Het invoerveld juist
andersom: verzonken in plaats van opstaand.

### Kleuren gemeten in plaats van geschat
PIL was niet beschikbaar, dus de PNG zelf gedecodeerd (zlib + de vijf
PNG-filters). Horizontale doorsnede dwars door een toets:

```
rug 37 | 17, 11 kier | 61 glans | 49 49 50 toetsvlak | 40 | 17, 12 kier
```

en verticaal: kier 12 -> glans 80 -> vlak 49. Dat bevestigde de opbouw: het
donker zit **buiten** het lichte lijntje. De glans is bovenaan sterker (80) dan
links (61), wat klopt met licht van boven.

| | |
|---|---|
| toetsvlak | `0x313131` |
| glans (boven + links) | `0x505050` |
| kier / schaduw (rondom) | `0x0C0C0C` |
| rug van het toetsenbord | `0x222222` |

### Een primitief in plaats van losse lijnen
`IDisplayDraw::fillBeveledRect(topLeft, size, colFg, colHighlight, colShadow,
style)`: schaduwrand rondom, glanslijn daarbinnen. `BevelStyle::Raised` zet die
langs boven en links, `Sunken` langs onder en rechts - want in een uitsparing
valt het licht op de tegenoverliggende binnenwand. Kleuren omwisselen zou niet
werken: de schaduw loopt rondom, alleen de glans verhuist.

Drie kleuren plus een stijl passen exact in de bestaande `uintA/uintB/uintC` en
`intA` van `DisplayCommand`. Dat is de reden voor een primitief in plaats van
zes `fillRect`-aanroepen vanuit `Panel`: bij 31 toetsen scheelt dat ~155
commando's per hertekening, en `CommandBufferSize` is 300.

`Panel` kreeg `setBevel()`, standaard uit. Het keyboard zet hem op elke toets en
op zijn eigen rug (die is zelf ook een toets), hoekradius op 0.

### Wat bijna stukging: opake tekst eet de rand op
Het invoerveld tekent zijn label opaak, en dat gereserveerde vak besloeg het hele
paneel. De blank-strook zou de bevel-lijntjes bij de eerste toetsaanslag hebben
overgeschilderd - rand zichtbaar tot je typt, dan weg. `Text` kreeg daarom een
marge op dat vak, gevuld door `TouchscreenButton` met `Panel::getBevelInsetPix()`
(2 px: schaduwrand plus glanslijn). Omdat `drawStringOpaque` alles op dat vak
clipt, blijft de rand ook heel als de tekst ertegenaan loopt.

## 2026-07-26 — Flikkervrij label vervangen, zonder buffer

### Vraag
Heeft de displaylaag faciliteiten tegen flikker? **Nee, geen enkele.**
`IDisplayDraw` kent alleen directe teken-primitieven; elke widget schildert
onmiddellijk naar de ILI9341 volgens painter's algorithm. `AsyncDisplay` lijkt op
buffering maar serialiseert alleen commando's van meerdere taken. De ILI9341
heeft geen tweede GRAM-pagina. Het enige wat TFT_eSPI biedt is `TFT_eSprite`, en
die wordt nergens gebruikt behalve in `main/spi_benchmark.h`.

Randvoorwaarde was **geen buffer**. Dat sluit sprites en double buffering uit;
dan blijft er één techniek over: *niet wissen wat je gaat overtekenen*.

### Gemeten oorzaak
`Inputfield::updateButtonLabel` deed `Base::show(true)`. Per toetsaanslag, met
`CRT_ASYNCDISPLAY_COMMAND_TIMING` + auto-typer:

```
fillRect     2567-2572 us   (constant: het hele veld, 256x24 = 6144 px)
drawString    335-1355 us   (groeit met de tekstlengte)
```

De 2,57 ms fill komt exact overeen met 6144 px x 409 ns/pixel. Het veld stond dus
**2,9 tot 3,9 ms leeg** per aanslag, in twee losse queue-commando's.

### Na de fix
```
drawStringOpaque  3064-3946 us   (een enkel commando)
```
Totale bustijd gelijk - dezelfde pixels worden geschreven - maar nu *met de tekst
er al in* in plaats van eerst leeg en dan gevuld. De veldbrede wis is weg.
Visueel bevestigd: knippert niet meer.

### Correctie op de eerste aanname
Het plan stelde dat het lege venster "nul per constructie" zou worden. Dat klopt
niet. Bij free fonts vult TFT_eSPI eerst de hele tekstcel en tekent daarna de
glyphs erin (`TFT_eSPI.cpp:5039`). Er blijft dus een wis, maar die beslaat de
tekstcel in plaats van het hele veld, en er zit geen tweede commando achteraan.

### Wat setTextPadding wel en niet doet
Eerst leek `setTextPadding()` onbruikbaar voor free fonts: `padding = 101`
(`TFT_eSPI.cpp:4938`) terwijl de blank-switch alleen 1/2/3 kent. Maar op
`:5041` staat `padding -= 100`, binnen de tak die opake tekst afhandelt. Hij
werkt dus wél - alleen niet voor een lege string, want die tak eist ook dat het
eerste glyph binnen het fontbereik valt. Juist die lege string heeft `hideText()`
nodig. Daarom blankt de adapter zelf, uniform voor beide fonttypes.

### Twee zichtbare artefacten, en wat ze blootlegden
1. **Balk leek te hoog behalve links.** De blank-strook was `fontHeight` (29 px)
   hoog in plaats van de paneelhoogte (24), dus er werd grijs *buiten* het paneel
   geschilderd. Opgelost met `setViewport` om het gereserveerde vak, hetzelfde
   mechanisme als in `drawImage`.
2. **Na de eerste letter verdween het effect links.**
   `applyOffsetForLowerCaseText` zet de x van het label op 0 en gooit daarmee de
   inspringing weg die `setSizeOfParent` aan een MidLeft-label geeft; de tekst
   sprong dus naar de linkerrand en de strook dekte opeens de volle breedte.
   Zo gelaten: die inspringing is `fontHeight`, een verticale maat als
   horizontale marge, en breder dan een letter.

### De onderliggende vondst: het font past niet
```
panel 256x24, fontHeight 29
```
FSB12 heeft een glyph-cel van 29 px in een veld van 24. Transparante tekst
verborg dat door simpelweg buiten de widget te tekenen; zodra tekst zijn eigen
achtergrond meebrengt moet je kiezen tussen 5 px grijs buiten het veld of
clippen. `TouchscreenButton` waarschuwt dit nu expliciet, en het voorbeeld geeft
het invoerveld 140 promillage (33 px) in plaats van 100 (24 px).

### Losse observatie: sdkconfig is grondiger teruggevallen dan gedacht
Naast `CONFIG_FREERTOS_HZ` bleken ook de partitietabel
(`partitions_singleapp.csv` in plaats van de eigen `partitions.csv`) en de
flashgrootte (2MB terwijl de chip er 4 heeft) op defaults te staan. Zonder de
spiffs-partitie op 0x290000 kan `/TouchCalData1` niet worden bewaard, dus vroeg
het bord bij elke boot om touchkalibratie. Na het herstellen van beide bleek de
oude kalibratiedata er nog gewoon te staan: `[226, 3403, 319, 3413, 7]`.
**Overweeg een `sdkconfig.defaults` in de repo** - `sdkconfig` staat in
.gitignore, dus deze reeks gaat zich anders herhalen.

## 2026-07-26 — Images in CleanGUI: bufferloos 16bpp blitten uit flash

### Doel
Een `Panel` kan bij constructie een image meekrijgen en tekent dan die image in
plaats van een gekleurde rechthoek. Rechtstreeks 16bpp blitten uit `const`
geheugen, zónder tussenbuffer; bij transparantie de runs van niet-transparante
pixels per blok versturen. CleanGUI blijft display-adapter-onafhankelijk.

### Uitkomst van het vooronderzoek: géén TFT-sprite nodig
TFT_eSPI heeft alles al in huis:
- `pushImage(x,y,w,h,uint16_t*)` (TFT_eSPI.cpp:1361) schrijft een ongekapt beeld
  in één `pushPixels(data, w*h)` — nul kopieën, van .rodata rechtstreeks de
  SPI-FIFO in.
- De transparante overload (TFT_eSPI.cpp:1391) verzamelt per beeldregel runs van
  niet-transparante pixels en schrijft elke run weg met één `setWindow()` +
  `pushPixels()`. Precies de gewenste per-pixel/per-blok-splitsing.
Een sprite ertussen zou alleen een kopie en `2*w*h` bytes DRAM toevoegen.

Twee bevindingen die het ontwerp bepaalden:
- **De `const`-overload is nét niet de snelste.** Regel 1450 kopieert eerst regel
  voor regel naar een stack-lijnbuffer en pusht dán. Die splitsing bestaat voor
  AVR, waar PROGMEM een aparte adresruimte is. Op de ESP32 is flash
  memory-mapped en `pushImage` leest alleen — een `const_cast` in de adapter is
  hier dus veilig en levert de echte bufferloze weg op.
- **Byte-volgorde kost niets.** `pushSwapBytePixels`
  (Processors/TFT_eSPI_ESP32.c:288) draait de bytes om terwijl de vorige
  512-bits transfer nog loopt: 16 shuffles tegen 12,8 µs bustijd, volledig
  verstopt. Data dus in host-volgorde houden en `setSwapBytes(true)` zetten
  (netjes terugzetten na afloop, want het is globale state op `_tft`).

### Wat is toegevoegd
- **`libs/CleanGUI/src/crt_Image.h`** (nieuw): `enum class PixelFormat`,
  `class Image` (opaque handle: `pData`, `width`, `height`, `format`,
  `bHasTransparency`, `colTransparent` in ARGB), plus de vrije functie
  `calcAlignedTopLeftPix()`.
  CleanGUI raakt de pixels nooit aan; de format-tag laat een andere adapter
  weigeren of converteren. Bewust géén abstracte `IImage` met `getPixel()` —
  dat zou elke pixel virtueel maken. `Image` heeft `constexpr` constructors, dus
  een image kan volledig in flash staan.
- **`IDisplayDraw`** kreeg er één functie bij:
  `drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment)`.
  Semantiek: plaats de image binnen de rechthoek volgens `alignment` en **clip**
  hem daarop. De uitlijnrekensom zit in `calcAlignedTopLeftPix()` in CleanGUI —
  gedeeld, niet per adapter gekopieerd. Er is bewust géén aparte clip-rechthoek
  in de signatuur: de doelrechthoek *is* de cliprechthoek, waardoor het commando
  in de bestaande `vecA`/`vecB`/`intA`-velden van `DisplayCommand` past.
- **`Panel`**: extra ctor-overload met `const Image&`, plus `setImage()`/
  `getImage()`. De image wordt uitgelijnd met de al bestaande `Alignment` van de
  widget (die voor Panel tot nu toe ongebruikt was).
- **Drie implementors bijgewerkt** (dat is de complete lijst):
  `TFT_eSPI_DisplayAdapter`, `AsyncDisplay`, `DisplayCommand`
  (`drawImage=15` plus één `const Image*`-veld; lifetime-afspraak identiek aan
  die van `str`, en voor const statics in flash automatisch waar).

### Clipping: via de viewport van TFT_eSPI
`setViewport(x, y, w, h, false /*vpDatum*/)` zet de cliprechthoek zonder de
oorsprong te verschuiven, waarna de `PI_CLIP`-macro binnenin élke `pushImage`
het kappen doet. Daarna `resetViewport()`. Geen eigen clip-rekenwerk, en het is
exact hetzelfde pad dat de sprite-code van de library gebruikt.

### Achtergrond niet vooraf vullen
`Panel::needsBackground()` slaat de `fillRect`/`fillSmoothRoundRect` over als er
een image is die de panel volledig bedekt (verspilde bustijd) én altijd als de
image transparantie heeft — daar zijn juist de pixels van de ouder eronder wat
zichtbaar moet blijven. Gevolg: een dekkende image die de panel vult onderdrukt
ook de afgeronde hoeken; een rechthoekige blit kan die nu eenmaal niet honoreren.

### Meegenomen foutje
De copy-ctor en `operator=` van `DisplayCommand` kopieerden `intB` niet — het
veld waar `drawString` de alignment in stopt. Het viel niet op omdat
`Queue::write` via FreeRTOS een rauwe memcpy doet. Bij het toevoegen van
`pImage` alsnog rechtgezet.

### Voorbeeld
`libs/CleanGUI_for_TFT_eSPI/examples/Panel` uitgebreid met drie image-panels:
- `crt_TestPanelImages.h` (gegenereerd): `imageGradient` (64x48, dekkend, met
  witte rand en zwarte diagonaal) en `imageRing` (64x64, schietschijf met
  magenta 0x00FF00FF als transparante kleur). Samen 14.336 bytes in .rodata.
- panel groter dan de image, `MidMid` → gecentreerd met blauwe rand eromheen;
- panel kleiner dan de image, `BottomRight` → zichtbaar gekapt;
- transparante ring als kind van een groen achtergrondpaneel → het groen blijft
  door de transparante delen heen zichtbaar.

### Verificatie
`idf.py build` compileert schoon met `#include <Panel.ino>` in main.cpp.
`nm` bevestigt dat beide pixelarrays én beide `Image`-objecten op 0x3F40xxxx
liggen (flash-gemapte .rodata): nul bytes DRAM.
Nog **niet** op de CYD getest — `/dev/ttyUSB0` bestond niet meer na de
WSL-herstart (usbipd opnieuw attachen).

### Meting: alles is busgelimiteerd, behalve de transparante blit
Het voorbeeld tekent nu zo snel als de bus toelaat en rapporteert elke 2 s een
exponentieel voortschrijdend gemiddelde (`ema += (measured - ema) / 16`, geen
history-buffer). Elk paneel wordt apart geklokt. Resultaat, rotsvast:

```
17.85 fps | clear 31.44 ms
  show (us): red 2685 | centered 3833 | clipped 1021 | backdrop 12745 | ring 3145
```

Referentiesnelheid uit de fullscreen clear zelf: 76.800 px / 31.440 us =
**409 ns/pixel**. Daarmee klopt de boekhouding tot op 0,5%:

| onderdeel | gepushte pixels | bus-verwacht | gemeten | premie |
|---|---|---|---|---|
| clear 320x240 | 76.800 | (referentie) | 31,44 ms | 1,00 |
| rood paneel 80x80 | 6.400 | 2,62 ms | 2,68 ms | 1,02 |
| centered: fill 84x68 + image 64x48 | 8.784 | 3,59 ms | 3,82 ms | 1,06 |
| clipped: 40x40 uit de image | 1.600 | 0,65 ms | 1,02 ms | 1,56 |
| backdrop fillSmoothRoundRect 230x130 | 29.900 | 12,23 ms | 12,72 ms | 1,04 |
| ring, transparant | 1.592 | 0,65 ms | 3,06 ms | **4,70** |

Totaal 54,74 ms + 1 ms `vTaskDelay` = 55,74 ms -> 17,94 fps (gemeten 17,85).

**`fillSmoothRoundRect` is niet traag.** Hij zit 4% boven het busplafond, dus hij
vult het interieur gewoon met blokken en anti-aliast alleen de hoekbogen. De
eerdere verdenking (per-pixel anti-aliasing, 581 ns/pixel) klopt niet.

**De enige echte premie zit op transparantie: 4,7x.** Niet per pixel maar per
run: de ring heeft 1.592 niet-transparante pixels verdeeld over **200 runs**, en
elke run kost een `setWindow`. 2,4 ms extra / 200 = ~12 us per setWindow. Een
transparante image is dus goedkoop zolang de runs lang zijn, en duur bij veel
losse eilandjes. Dat is het echte ontwerpcriterium, niet het pixelaantal.

### Correctie op een vuistregel in CLAUDE.md
De regel "`pushImage` haalt 12,4 ns/pixel, `drawPixel` 581 ns/pixel" komt uit de
tabel in de entry van 2026-07-20, waar die getallen `spr.pushImage()` en
`spr.drawPixel()` heten: **sprite-operaties in geheugen**, geen SPI-uitvoer.
CLAUDE.md heeft de `spr.`-prefix laten vallen, waardoor het leest als
display-doorvoer. 12,4 ns/pixel = 161 MB/s en is over een 40 MHz-bus fysiek
onmogelijk. Het busplafond is ~400 ns/pixel (2 bytes bij 40 MHz plus overhead);
diezelfde entry zegt dat zelf ook al: 64x64 pushen naar het scherm kost 1645 us,
oftewel 402 ns/pixel.

### Helpt een release-build? Nauwelijks -- en dat bevestigt de diagnose
Gemeten met `CONFIG_COMPILER_OPTIMIZATION_PERF` (-O2) + assertions uit,
tegenover de debug-build (`CONFIG_COMPILER_OPTIMIZATION_DEBUG`):

| | debug | release | verschil |
|---|---|---|---|
| clear 320x240 | 31,44 ms | 31,30 ms | -0,4% |
| rood paneel | 2680 us | 2674 us | -0,2% |
| backdrop roundrect | 12724 us | 12717 us | -0,1% |
| centered + image | 3816 us | 3753 us | -1,6% |
| clipped image | 1021 us | 922 us | **-9,7%** |
| transparante ring | 3059 us | 2645 us | **-13,5%** |
| totaal | 17,85 fps | 18,17 fps | +1,8% |

De drie busgelimiteerde posten bewegen niet -- daar wacht de CPU op de SPI.
Precies de twee posten met per-run CPU-werk winnen 10-14%. Onafhankelijke
bevestiging dat de 4,7x premie op de transparante blit de setWindow-aanroepen
zijn en niet iets aan de bus.

Let op: `CRT_HIGH_WATERMARK_INCREASE_LOGGING` (de stack-dumps, ~0,25 ms per
lus-iteratie) speelt in dit voorbeeld geen rol -- `TestPanel::main` roept
`dumpStackHighWaterMarkIfIncreased()` niet aan. Waar dat wel gebeurt is het een
heel ander verhaal, met name `crt_AsyncDisplay.h:51`: daar gaat hij af per
verwerkt teken-commando.

sdkconfig staat weer op debug; 1,8% weegt niet op tegen asserts tijdens de
ontwikkeling.

### De echte hefboom voor de framerate
De fullscreen clear is 31,4 van de 55,7 ms (56%) en wordt daarna grotendeels
weer overtekend door de panelen. Niet elk frame wissen -- of alleen wissen wat
geen paneel bedekt -- verdubbelt de framerate zowat. Aan de panelen zelf valt
weinig te halen: die zitten al op de bus vast.

### Losse observatie
`sdkconfig` (staat in .gitignore) was teruggevallen op defaults, met
`CONFIG_FREERTOS_HZ=100`; arduino-esp32 eist 1000. Handmatig teruggezet, anders
komt de build niet eens door cmake heen.

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

