# Claude log

## 2026-07-28 — Alle voorbeelden in PAL8, en een palet dat een mens kan lezen

Alle acht voorbeelden zijn op de PAL8-adapter gezet en een voor een op de CYD
gedraaid. Dat leverde vooral iets anders op dan verwacht: niet fouten in de
adapter, maar fouten in de *voorbeelden* die de directe adapter al die tijd had
toegedekt.

### Waarom de magenta index zoveel vond
`begin()` maakt het scherm een keer zwart, en op zwart valt een stuk scherm dat
door niemand wordt getekend niet op. Een backbuffer die met de transparante index
begint wel: die is magenta. Drie keer dezelfde soort vondst op een dag —
MultiPanel had het vorige week, vier voorbeelden bleken helemaal geen
achtergrondpaneel te hebben (AsyncDisplay, TouchscreenButton,
TouchscreenButtonGroup, TouchscreenKeyboard), en TouchscreenKeyboard riep
bovendien `touchscreenKeyboard.show(true)` in plaats van `page1.show(true)`,
zodat het paneel dat er inmiddels wel was nooit aan de beurt kwam.

Dat laatste was fraai zichtbaar: bij enter *werd* het gebied wel zwart, want de
`hide()` van het toetsenbord liet de ouder herstellen. Het paneel bestond dus,
het was alleen nooit getoond.

### present() stond in maar twee van de acht
Panel en MultiPanel riepen `present()` aan; de andere zes zijn geschreven voor de
directe adapter, waar die leeg is. Ze tekenden keurig in de buffer en stuurden
hem nooit uit - zwart scherm. Per voorbeeld een regel toevoegen zou zes keer
hetzelfde zijn en de zevende keer vergeten worden, dus het zit nu in
`AsyncDisplay::main()`: is de commandoqueue leeg, dan is het beeld af.

```cpp
if (bBufferDirty && (queueCommands.getNofMessagesWaiting() == 0) && display.isBuffered())
```

Drie voorwaarden, elk met een reden. `bBufferDirty` staat uit zodra het laatste
uitgevoerde commando zelf een `present` was, anders stuurt MultiPanel het scherm
twee keer per beeld uit en dat is 31 ms per keer. De queue-check voorkomt dat een
half beeld eruit gaat als de batchlimiet van tien commando's wordt geraakt. En
`isBuffered()` is onwaar bij RGB565, dus daar verandert er niets.

### Panel had te weinig stack
Boot loop met een `LoadProhibited` in een malloc diep in `TFT_eSPI::init()`.
Panel is het enige voorbeeld dat zijn adapter als *lokale* variabele in `main()`
maakt, dus die staat op de taakstack. Met de compiler nagemeten:
`sizeof` RGB565-adapter 984 bytes, PAL8 **2744** - de drie paletkopieen erin zijn
samen al 1280. Op 4000 bytes stack past dat niet meer; staat nu op 10000.

### Het palet: transparant naar 255, en een handgeschreven _userPalette.h
Aanleiding was dat er over het palet niets viel *af te spreken*: median-cut over
de plaatjes bepaalde alles, dus `paletteIndex(N)` was onbruikbaar zodra er een
plaatje bijkwam. De indeling is nu:

```
0 .. N-1       letterlijk uit _pal8/_userPalette.h   (ligt vast)
N .. N+M-1     median-cut over de plaatjes
N+M .. 254     opvulling: grootste kubus die past, daarna grijzen
255            transparant (magenta)
```

Transparant is naar het *eind* verhuisd zodat `arUserPalette[i]` samenvalt met
index `i`. Dat is de hele ergonomische winst; met transparant op 0 schoof alles
een op.

`arUserPalette` staat in ARGB (`0x00RRGGBB`), niet in RGB565 - het bestand is er
juist om met de hand geschreven te worden, en zo kun je een kleur letterlijk uit
een widget overnemen. De converter rekent om.

De opvulling is bewust geen zwart. `indexForColour()` zoekt over alle 255
ingangen, dus een staart vol zwart trekt elke donkere kleur naar puur zwart, ook
als de gebruiker bewust geen zwart koos. Een kubus dekt de ruimte gelijkmatig.
Bij MultiPanel gebeurde dat meteen: de quantizer gaf maar 156 kleuren terug op
255 gevraagd, dus 99 ingangen kregen kubus en grijzen.

Wat dit oplost: de widgets leiden **geen** kleuren af tijdens het draaien - `grep`
op `darken|lighten|blend` over `libs/CleanGUI/src` geeft niets, de bevelkleuren
zijn letterlijke waarden. Staan die exact in het palet, dan is de nearest-match
voor de hele GUI exact in plaats van benaderd. Precies wat Inputfield nodig had,
want dat voorbeeld leent zijn plaatjes van Panel en kreeg daarvoor een palet dat
uitsluitend uit een gradient en een ring was gekwantiseerd.

Magenta (`0x00FF00FF`) staat er met opzet *niet* in: het is nergens een
tekenkleur, alleen een transparantiemarkering, en in het palet zou het de
"magenta betekent niet-getekend"-diagnose ondermijnen.

Bijbehorend: `allocateBackBufferIfNeeded()` vult de buffer nu met index 255 in
plaats van 0, anders was die diagnose weg zodra index 0 een gewone
gebruikerskleur werd.

### De bevelkolom van het invoerveld
Klein artefact in Inputfield: zodra je typte werd de linker bevelkolom van het
invoerveld vanaf een bepaalde hoogte weggeveegd. Twee metingen brachten het
thuis - eerst de geometrie van `drawStringOpaque`, daarna de pixels zelf:

```
y=24: ... 3   3   3 ...     bovenrand van het veld (index 3 = 0x303030)
y=25: ...(x=32) 3  10 ...   alleen x=32 nog donker
y=32: ...(x=32)10  10 ...   hier is de bevelkolom weg
```

`Text::calcPaddingSizePix()` kortte de *hoogte* aan twee kanten in (`2*m`) maar de
*breedte* maar aan een kant (`m`), en die ene ging er rechts af. Links bleef er
niets over, dus de gereserveerde box begon precies op de bevelkolom. Verticaal
ging het daardoor al goed en horizontaal niet - vandaar dat alleen de linkerrand
sneuvelde.

Opgelost door het tekstdatum bij opake tekst een marge naar binnen te schuiven
(`calcOpaqueOffsetXPix()`); de adapter leidt de box uit dat datum af, dus die
schuift mee. De breedte gaat nu met `2*m` omlaag, want anders loopt de box aan de
overkant weer over de bevel. Gecentreerde tekst blijft ongemoeid: die ligt al
symmetrisch om zijn datum.

Waarom pas een lege regel het herstelde: de rechterstrook begint op
`textTopLeft.x + textWidthPix` en raakt kolom 32 nooit - behalve bij lege tekst,
want dan begint hij op 32 zelf.

### Schuifanimatie met een deadline
Het toetsenbord van Inputfield schoof zo snel binnen dat er niets van te zien
was. Geen vaste pauze per stap, maar een deadline per stap met `waitUntilDrawn()`
ervoor: een `present()` kost hier al ~31 ms per stap en bijna niets bij
rechtstreeks tekenen, dus een vaste pauze zou de duur adapterafhankelijk maken.
Nu op 400 ms - en dat is ongeveer de vloer, want ~13 stappen x 31 ms is al 0,4 s.
Lager zetten verandert niets meer; daarvoor moet `stepPix` omhoog.

### Gemeten en afgevinkt
`getScreenSize()` geeft inmiddels gewoon 320x240 - het openstaande punt uit de
entry van 2026-07-26 is in de adapter rechtgezet.

### Twee bestaande fouten in ImageViewer, allebei door meten gevonden
Geen van beide had met het palet te maken; ze zaten er al.

**Een knop bleef na indrukken voorgoed verzonken.** De log gaf het meteen weg:
alleen `pressed=1`, nooit een `pressed=0`. Het loslaten kwam dus niet aan.

`TouchscreenButton::enable()` wiste `bPressed`, en `Widget::show()` roept
`enable(false)` aan. Wie de knop ingedrukt tekende, wiste daarmee tegelijk het
feit dat hij was ingedrukt, waarna `touchReleased()` niets meer stuurde -
`if (bPressed)` was al onwaar. Wissen gebeurt nu alleen nog bij een echte
overgang van uit naar aan, wat de oorspronkelijke bedoeling van die regel was.

**De labels van `r-` en `c-` ontbraken tot je ze indrukte**, die van `r+` en `c+`
niet. Die asymmetrie was de aanwijzing. Gemeten in `redrawRegion()`:

```
regio  pos=( 89,157) size=(141, 77)
r-     pos=(136,157) size=(  0,  0)
c-     pos=( 89,185) size=(  0,  0)
```

*Alle vier* de labels hebben maat nul, maar de cull-test
`rectsIntersect(getGlobPosPix(), _sizePix, ...)` wordt daarmee
`regioTop < labelY` en `regioLinks < labelX`. `r-` ligt op de bovenrand van de
hertekende regio en `c-` op de linkerrand, dus daar staat `157 < 157` en
`89 < 89`. De plus-knoppen liggen verderop en overleefden het toevallig. Rakende
randen tellen met opzet niet mee (zie `crt_typesAndConstants.h`), en dat is
precies wat een rect van niets fataal maakt.

Waarom de maat nul was: `Text::updateSize()` draait alleen vanuit `setFont`,
`setFontScale` en `setText`, en die laatste alleen als de string *verandert*. Een
label dat zijn tekst bij constructie krijgt en nooit wijzigt, komt langs geen van
de drie - en bij constructie is er ook nog geen display, waar `updateSize()`
sowieso op afbreekt. `Text` haakt nu in op `setDisplay()`: het eerste moment
waarop de tekst te meten valt.

### Werkwijze die zich terugbetaalde
Vier artefacten, vier keer eerst meten. Elke keer wees de redenering vooraf de
verkeerde kant op:

- Blauwe randjes bij groene knoppen: er bleek helemaal niets te blenden - de
  adapter kreeg maar drie kleuren binnen en de buffer had harde overgangen. Het
  was de magenta achtergrond ernaast, die tegen groen wel opvalt en tegen rood
  niet.
- Het kolommetje in het invoerveld: niet de blanking (die tekende daar nul
  pixels) maar de gereserveerde box, die op de bevel begon.
- Zwart scherm bij TouchscreenButtonGroup: een los stekkertje.
- De labels: niet de regio - die klopte precies - maar een rect van niets.

Instrumentatie die daarbij hielp en er weer uit is: kleur-naar-index in
`indexForColour()`, een pixeldump uit `present()`, de geometrie van
`drawStringOpaque()`, en de cull in `redrawRegion()`.

### 80 MHz geprobeerd, en afgesloten
Het laatste openstaande punt uit de speed-research. `SPI_FREQUENCY` op 80 MHz
(80/1) gezet en MultiPanel gedraaid:

| sprites | 40 MHz | 80 MHz |
|---|---|---|
| 0 | 29 | **51** |

Doorvoer is dus geen probleem - het bord komt gewoon op en meldt vrolijk 51 fps.
**Maar het beeld is stuk.** Dat is precies het falen dat we verwachtten: niet een
crash, maar ruis en verkeerde kleuren, terwijl de seriële poort blijft doen alsof
er niets aan de hand is. Zonder ernaar te kijken had dit als winst geteld.

Iets ertussenin bestaat niet. De ESP32 deelt de 80 MHz APB-klok door een geheel
getal, dus 80/1, 80/2 (=40), 80/3 (=26,67) en 80/4 (=20) zijn de enige echte
waarden; voor iets tussen 40 en 80 zou de deler tussen 1 en 2 moeten liggen. Een
`60000000` in User_Setup.h levert stilzwijgend 40 MHz op. Terug op 40 dus, met de
reden erbij in het bestand zelf.

Wel iets geleerd over waar de tijd nu zit. Bij 80 MHz halveert de busvloer van
31,6 naar 15,8 ms, wat zuiver busgelimiteerd ~63 fps zou geven. Gemeten werd 51
fps = 19,6 ms. Dat verschil is het uitpakken van het palet, dat bij 40 MHz
volledig onder de transfer wegviel en er nu bovenuit steekt. **De bus is niet
langer het enige plafond** - een aardige correctie op de vuistregel in CLAUDE.md
dat vrijwel al het tekenwerk busgelimiteerd is. Die geldt bij 40 MHz; bij 80 niet
meer.

### Nog open
Niets meer uit de oude lijst.

## 2026-07-28 — DMA: uitpakken en uitzenden tegelijk

`present()` was de rem: 38,4 ms, waarvan 31,6 ms busvloer en ~6,8 ms CPU (het
palet toepassen, en de FIFO voeren). Met DMA hoeven die niet meer op elkaar te
wachten - terwijl strook N over de bus gaat, pakt de CPU strook N+1 uit. Het
logboek zei het al: *"DMA loont pas als er ook CPU-werk per frame is om mee te
overlappen."* Dat was er nu eindelijk.

### Wat het werd
- `initDMA()` zodra het paneel draait, en `startWrite()` erachteraan om CS laag te
  houden. Dat laatste is nodig omdat `setAddrWindow` hem anders tussen de stroken
  door loslaat, en omdat `endWrite()` op de laatste transfer zou wachten - precies
  de wachttijd die we willen vermijden. Buiten `present()` raakt niets anders het
  paneel: al het tekenwerk gaat naar de buffer.
- **Twee** strookbuffers uit `MALLOC_CAP_DMA` (20 KB), waar `present()` tussen
  pingpongt. De `dmaWait()` zit binnenin `pushImageDMA` en wacht op de *vorige*
  transfer, niet op de zijne; dat is precies de pijplijn.
- De const-variant `pushImageDMA(x,y,w,h, const uint16_t*)` met opzet: die
  kopieert niet en wisselt geen bytes om, en dat laatste hoeft niet want het palet
  staat al omgedraaid klaar.
- Mislukt `initDMA()`, dan pusht `present()` gewoon blokkerend verder.

### Het risico dat niet uitkwam
`initDMA()` roept `spi_bus_initialize(HSPI_HOST, ...)` aan met `ESP_ERROR_CHECK`
eromheen: was die bus al door de Arduino-laag geclaimd via dezelfde IDF-driver,
dan was dat geen foutcode maar een abort. Daarom eerst alleen dát aangezet en
geflasht. Hij geeft `true` en het bord komt gewoon op - de Arduino-laag praat via
`esp32-hal-spi` met de registers, niet via `spi_master`.

De touch kan er ook niet mee in de knoop raken: met `TOUCH_SCLK` gedefinieerd
gebruikt `Touch.cpp` zijn eigen `touch_spi_transfer()` met `digitalWrite`, op
eigen pinnen. De `spi.transfer`-tak eronder is voor borden die de displaybus
delen; die van ons niet.

### Gemeten

| sprites | zonder DMA | met DMA |
|---|---|---|
| 0 | 24 | **29** |
| 5 | 21 | 27 |
| 20 dekkend | 20 | 26 |
| 20 transparant | 18 | 22 |

Zo'n 25% erbij over de hele linie, en de curve blijft vlak: 29 -> 22 over nul tot
twintig sprites. Ter herinnering waar dit begon: dezelfde twintig transparante
sprites deden 3 fps op de directe adapter met `moveTo`.

`present()` zit nu op ~33 ms tegen een busvloer van 31,6. Daarmee is dit het
eindpunt van deze weg: zonder een tweede SPI-bus of een hogere klok valt er niets
meer te halen.

## 2026-07-27 — De PAL8-backbuffer werkt

`TFT_eSPI_DisplayAdapter_PAL8`: een schermvullende 8bpp-sprite waarvan wij de
bytes als palet-indices lezen. Tekenen gaat via TFT_eSPI zelf - inclusief tekst
met free fonts, wat de reden was om op een `TFT_eSprite` te blijven in plaats van
een eigen `uint8_t`-buffer.

### De dragerkleur
Elke 8bpp-schrijfactie in TFT_eSPI reduceert de kleur met
`(c & 0xE000)>>8 | (c & 0x0700)>>6 | (c & 0x0018)>>3`. Die is exact omkeerbaar,
dus voor elke index bestaat er een kleur die naar precies die index reduceert.
`colTo16bit()` - nu virtueel - geeft die dragerkleur terug in plaats van een echte
kleur, en TFT_eSPI schrijft daarmee onze index.

Bij het opstarten worden alle 256 indices erin geschreven en met
`readPixelValue()` teruggelezen. Lukt dat niet, dan valt de adapter terug op het
paneel en zegt waarom. De aanname over TFT_eSPI's binnenwerk is daarmee een
gecontroleerd feit; zonder die controle zou een toekomstige versie van de
bibliotheek dit stilzwijgend kunnen breken.

Vijf primitieven (`fillScreen`, `drawRect` en de drie rounded rects) heeft een
sprite niet zelf en zijn niet virtueel, dus die zijn hier opgebouwd uit
`fillRect`. Afgeronde hoeken zijn voorlopig vierkant.

### Gemeten, met de auto-spawn van eerder

| sprites | RGB565 | PAL8-backbuffer |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 10 dekkend | 39 | 20 |
| 20 dekkend | 16 | 14 |
| 20 transparant | 3 | 4 |

Wat hier staat is precies wat een backbuffer doet: **de beeldsnelheid wordt
vlak.** Van 0 naar 20 sprites zakt hij van 24 naar 14 fps, waar de directe
adapter van 100 naar 16 duikelt. Bij weinig op het scherm is de backbuffer dus
duurder en bij veel goedkoper - het omslagpunt ligt rond de 20 dekkende sprites.
En het flikkeren en de wisselende volgorde zijn weg, want er komt pas iets op het
scherm als het beeld af is.

### Index 0 is magenta, en dat is met opzet
In het palet dat de converter schrijft is index 0 gereserveerd voor transparant.
Die index wordt bij het blitten overgeslagen, dus je hoort hem nooit op het scherm
te zien - en juist daarom staat er magenta (0xF81F) in plaats van zwart.

Dat betaalde zich meteen terug. Bij de eerste PAL8-run was de strook rond de
knoppen in MultiPanel paars. Geen fout in de adapter: die pixels werden door geen
enkele widget getekend. De map loopt tot y=192, de knoppen beginnen op 196, en
daartussen was het scherm van niemand. Op het paneel viel dat nooit op, omdat
`begin()` het scherm een keer zwart maakt en dat zwart daar toevallig goed
uitzag. Een backbuffer die met index 0 begint, maakt zo'n gat zichtbaar.

Was de buffer met "zwart" geinitialiseerd, dan had het gat er nog steeds gezeten
en had niemand het gezien - tot het moment dat de achtergrondkleur verandert.
MultiPanel heeft nu een schermvullend `backgroundPanel` onderop, zoals ImageViewer
al had.

Dezelfde regel geldt breder: **magenta in beeld betekent dat er iets niet is
getekend.** Niet dat de kleur mis is.

### Waar de rest van de tijd zit
Van de ~71 ms bij 20 dekkende sprites is ~40 ms de uitzending en ~31 ms het
tekenen in de buffer.

De transparante blit is apart gemeten, in de adapter zelf:

```
transp 64x64: 599 us per blit, 173 stukken, 3463 ns per stuk
```

Van die 599 us gaat vrijwel alles op aan de 173 aanroepen van
`TFT_eSprite::pushImage` (3,5 us elk). Het verplaatsen van de pixels zelf stelt
niets voor: 1.945 zichtbare bytes is bij DRAM-tempo een paar microseconden.
Anders gezegd: **99% van een transparante blit is opzet, 1% is werk.**

Die 3,5 us zit in de aanroep zelf, de `PI_CLIP`-macro die per aanroep opnieuw
tegen de viewport klipt, de keten van `_bpp`/`sbpp`-tests, en een `memcpy` van
gemiddeld elf bytes - die op zijn beurt ook aanroepkosten heeft.

Eerder stond hier 45 us per stuk. Dat was afgeleid uit de beeldsnelheid en dus
uit alles wat er verder in een beeld gebeurt; de directe meting zegt 3,5.

### En toen bleek het ergens anders te zitten
De rekensom klopte nog steeds niet: 40 ms uitzending plus ~15 ms blits is 55, en
er ging 105 ms in een beeld. Dus per commandotype opgeteld aan de tekenkant, met
een samenvatting per seconde in plaats van een logregel per commando:

```
per seconde: tekenen 923 ms, wachten 29 ms, overig 51 ms
   drawImage    n= 797    509 ms  (638 us elk)
   present      n=  10    384 ms  (38417 us elk)
   fillRect     n= 560     17 ms  (31 us elk)
```

**797 blits per seconde bij tien sprites en tien beelden per seconde: 75 blits
per beeld.** Niet tien. Elke transparante sprite laat via `repaintRegion` zijn
hele 64x64 opnieuw opbouwen, inclusief elke buur die daar overheen valt, plus de
vrijgekomen randstroken. Dat is de O(N^2) uit het z-verhaal van eerder, nu met
een getal erbij.

### De les: met een backbuffer is slim hertekenen dom
De hele `moveTo`-machinerie bestaat om buspixels te sparen. In een buffer is er
geen bus, en betaal je alleen de nadelen: buren die onnodig meegetekend worden.
Gewoon per beeld de hele tree opnieuw opbouwen kost tien blits in plaats van
vijfenzeventig.

`IDisplay::isBuffered()` erbij, zodat een voorbeeld eenmalig kan vragen wat voor
display het heeft en zijn aanpak daarop kiest. MultiPanel doet nu:

```cpp
if (_bDisplayIsBuffered) { setLocPos(...); }  // en daarna page1.show(true)
else                     { moveTo(...); }     // alleen de vrijgekomen strook
```

De adapter moest daarvoor het aanmaken van de buffer scheiden van het omleiden
van het tekenwerk: het antwoord op `isBuffered()` mag niet afhangen van wie er
toevallig het eerst wakker is, maar de kalibratie moet wel op het paneel blijven
tekenen omdat ze om aanrakingen vraagt.

### Eindresultaat

| sprites | RGB565, moveTo | PAL8-backbuffer, alles per beeld |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 5 | - | 21 |
| 10 dekkend | 39 | 21 |
| 20 dekkend | 16 | 20 |
| 20 transparant | 3 | **16** |

Van 3 naar 16 beelden per seconde, en de curve is bijna vlak: 24 -> 16 over nul
tot twintig sprites, waar de directe adapter van 100 naar 3 duikelt. De
commandowachtrij zakte van ~500 naar ~31, want de tekentaak loopt niet meer
achter de feiten aan.

Wat nu de rem is, is `present()` met 38,4 ms - het plafond ligt dus op 26 fps.

### De eigen blit, alsnog
Daarna toch gebouwd: klippen een keer per plaatje, dan `memcpy` per aaneengesloten
stuk rechtstreeks in de buffer, in plaats van `TFT_eSprite::pushImage` per stuk.

| 20 transparante sprites | fps |
|---|---|
| via `pushImage` per stuk | 16 |
| eigen klipping + `memcpy` | **18** |

Van de 62,5 ms per beeld ging ~24 ms naar tekenen, nu ~17 ms. Geen factor zes
zoals ik voorrekende: de resterende tijd zit in het aflopen van de rij op zoek
naar stukken (4.096 vergelijkingen per sprite) en in 173 kleine `memcpy`s. De
opzet per aanroep is weg, het scannen niet.

Drie begrenzingen gelden tegelijk en worden nu zelf uitgerekend: de rechthoek van
het panel, de clip-rect van `repaintRegion`, en de buffer. Dat is precies het soort
code waar een fout niet lelijk oogt maar buiten de buffer schrijft.

En dat pad werd door het voorbeeld nooit geraakt: de sprites ketsten netjes binnen
de map, en bij een volledige hertekening staat er geen clip-rect. Een eerste test
met sprites die 60 pixels over alle randen heen liepen bewees alleen dat het niet
crasht - visueel klopte hij niet eens, want CleanGUI klipt een kind niet tot zijn
ouder en die sprites tekenden dus over de knoppen heen.

De oplossing die het wel doet: **de sprites horizontaal met hun centrum op de rand
laten ketsen.** Daar valt de rand van de map samen met de rand van het scherm, dus
dan is het echte klipping, in normaal gebruik, en zichtbaar te controleren. Dat
staat nu zo in het voorbeeld - niet als test maar als gedrag, want ongebruikte
klipping is precies het soort code dat stilletjes verkeerd staat.

Verticaal blijven ze wel binnen de map: erboven ligt de fps-regel en eronder de
knoppen. Daarmee is `xFrom`/`xTo` in gebruik en gecontroleerd, en `yFrom`/`yTo`
niet. Zelfde code, andere as, dus het risico is klein - maar getest is het niet.

### Stand van zaken

| sprites | RGB565, moveTo | PAL8-backbuffer |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 10 transparant | - | 20 |
| 20 dekkend | 16 | 20 |
| 20 transparant | 3 | **18** |

Het plafond is nu `present()` met 38,4 ms, ofwel 26 fps. Wie verder wil: DMA laat
de expansie van de volgende strook overlappen met het uitzenden van de vorige, en
brengt dat richting de busvloer van 31,6 ms.

## 2026-07-27 — PAL8: gedeeld palet, indices in flash

Eerste twee stappen van de paletted weg. De keuze voor een palet in plaats van
RGB332 volgde uit de meting van hiervoor: de winst zat in de opzoektabel, en die
tabel heeft dezelfde vorm of je hem nu met het 3-3-2 raster vult of met
zelfgekozen kleuren. Een palet is dus gratis.

### Kleur of index, via één trechter
`paletteIndex(i)` in `crt_typesAndConstants.h` markeert de bovenste byte van een
ARGB-woord (die overal nul is) als "dit is een index". Daarmee betekent dezelfde
`fillRect()` zowel "maak dit lichtblauw" als "maak dit kleur 7", zonder de
interface te verdubbelen. Het landt allemaal in `colTo16bit()` van de adapter -
de enige plek waar een CleanGUI-kleur hardware-kleur wordt.

### De converter
`img2rgb565.py --pal8` leidt één palet af uit **alle** plaatjes van een
images-boom samen, en zet ze allemaal om naar indices daarin. Dat is met opzet
een operatie over de hele boom: alleen als plaatje en scherm hetzelfde palet
delen zijn de bytes van een plaatje al schermindices, en dat is wat blitten tot
kopieren terugbrengt. Index 0 is transparant, altijd - dat vervangt een
kleurvergelijking door een vergelijking met nul. Uitvoer in `images/_pal8/`,
naast de bestaande `_rgb565/`.

Voor de twee barbaren van MultiPanel: 156 kleuren uit 6.041 zichtbare pixels, en
8.192 bytes flash in plaats van 16.384.

### De les van vandaag: meet niet je eigen foutmelding
De eerste metingen zeiden dat een PAL8-plaatje 5,45 ms kostte tegen 1,88 ms voor
hetzelfde plaatje in RGB565. Bijna drie keer zo traag, en geen enkele verklaring
klopte: stroken pushen in plaats van regels hielp niet, het palet naar DRAM
verhuizen hielp niet, en de rekensom kwam op ~1,9 ms uit.

De oorzaak was dat er **helemaal niets werd getekend**. Bovenin `drawImage` stond:

```cpp
if (image.format != PixelFormat::RGB565)
{
    ESP_LOGI(..., "Unsupported PixelFormat: %d", ...);
    return;
}
```

PAL8 viel daar bij binnenkomst uit, en die 5,45 ms was de logregel zelf: op
115200 baud kost een regel van vijftig tekens ruim 4 ms. De meting was dus
volstrekt consistent (p90 lag 8 us van de mediaan) en volstrekt betekenisloos.

Die melding is nu eenmalig in plaats van per beeld, en de bewaking laat PAL8 door.
Wat de meting bruikbaar had gemaakt: de sprites waren onzichtbaar - dat was met
één blik op het scherm te zien geweest, en ik zat naar getallen te kijken.

### Wat het daarna wél doet
| | RGB565 | PAL8 op dezelfde adapter |
|---|---|---|
| 5 sprites | - | 40-60 fps |
| 20 dekkend | 16 fps | 8 fps |
| 20 transparant | 3 fps | 1 fps |

Ongeveer half zo snel, en dat hoort zo: het scherm is 16 bits, dus elk plaatje
moet alsnog per pixel door het palet, en transparant blitten kost een push per
aaneengesloten stuk. **PAL8 op een 16-bits doel is een compatibiliteitspad, geen
snelheidspad.** De winst komt pas als het doel zelf paletted is - dan is blitten
een `memcpy` per beeldrij. Dat is de volgende stap.

## 2026-07-27 — Aanzet tot een 8bpp backbuffer, en waarom hij (nog) niet werkt

### Wat er staat en werkt
- `IDisplayDraw::present()` door de hele keten: `AsyncDisplay` zet hem in de
  wachtrij, `DisplayCommand` voert hem uit, de directe adapter doet niets.
- De adapter is gesplitst in **het paneel** (`_tft`: begin, rotatie, touch,
  kalibratie) en **het tekendoel** (`_pTarget`). Hij heet nu
  `TFT_eSPI_DisplayAdapter_RGB565`, met een alias-template op de oude naam zodat
  `apps/pong_cleangui` en `CleanCore/examples/StdQueue` niets merken.
- Elk voorbeeld kiest met één regel:
  `using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_RGB565<...>;`
- `AsyncDisplay::waitUntilDrawn()`: wacht tot alles wat is ingestuurd ook is
  uitgevoerd. Nodig zodra present() duur wordt - zonder rem groeide de
  commandobuffer van 110 naar 843 in tien seconden (100 beelden per seconde
  insturen, 21 kunnen tonen) en sloeg de task watchdog aan. De **queue-diepte
  deugt niet als maat**: het commando dat op dat moment wordt uitgevoerd staat
  niet meer in de rij. Vandaar twee tellers.
- Alles geverifieerd: MultiPanel haalt met 20 dekkende sprites nog steeds 16-17
  fps, dus deze omweg kost in RGB565 niets.

### Waarom de RGB332-variant niet werkt
Aanname was: een `TFT_eSPI*` die naar een `TFT_eSprite` wijst tekent in die
sprite, want `TFT_eSprite` erft van `TFT_eSPI`. **Fout.** Van alle tekenmethoden
in TFT_eSPI zijn er precies acht virtueel - `drawPixel`, `drawChar`, `readPixel`,
`setWindow`, `pushColor` en drie hulpjes - met erboven: *"These are virtual so the
TFT_eSprite class can override them"*. De rest wordt door de sprite *verborgen*,
niet overschreven, dus via een basisklasse-pointer draait de paneelversie.

Van de 26 methoden die de adapter gebruikt: 16 zijn puur toestand (goed), 3 heeft
de sprite zelf (`fillRect`, `pushImage`, `width`), 2 lopen via de virtuele
`drawChar` (goed), en **5 landen op het paneel in plaats van in de buffer**:
`fillScreen`, `drawRect`, `drawRoundRect`, `fillRoundRect`,
`fillSmoothRoundRect`. Plus: `drawString` vult bij free fonts de glyph-cel met
`fillRect` (TFT_eSPI.cpp:5039) - ook niet-virtueel.

Vervolg zou zijn: de adapter templaten op het doeltype in plaats van een pointer,
plus eigen implementaties van die vijf, opgebouwd uit `fillRect`/`drawFastHLine`/
`drawPixel` die de sprite wel heeft.

### En het plafond ligt lager dan gedacht: gemeten uitsplitsing
Een schermvullende 8bpp-sprite uitsturen kost **46 ms, niet 31**. Dat is 21 fps,
geen 32. Waar die 14,6 ms extra in gaan (elk 76.800 pixels, gemiddelde van 5):

| meting | tijd | wat het toevoegt |
|---|---|---|
| `fillScreen`, egaal, geen brondata | 31.602 us | de busvloer |
| `pushImage` 16bpp, stroken van 8 regels (30 aanroepen) | 34.957 us | +3,4 ms: brondata ophalen |
| `pushImage` 16bpp, per regel (240 aanroepen) | 39.754 us | +4,8 ms: 210 extra aanroepen, ~23 us elk |
| `pushSprite` 8bpp, egale buffer | 46.209 us | +6,5 ms: de per-pixel lus |
| `pushSprite` 8bpp, ruis | 47.960 us | +1,75 ms: het schuifwerk zelf |

**De omzetting is niet de boosdoener.** Het rekenwerk (3-3-2 uitpakken naar
5-6-5) is met 1,75 ms het goedkoopste onderdeel; TFT_eSPI slaat het bovendien
over zolang de kleur niet verandert. Het geld zit in de per-pixel lus die twee
losse bytes in een regelbuffer schrijft (6,5 ms = 85 ns/pixel, zo'n 20 klokken
voor laden, vergelijken en twee byte-stores - `_lastColor` is een lid, dus geen
register) en in het per beeldregel pushen (4,8 ms).

Wat daaraan te doen zou zijn: een tabel van 256 `uint16_t` in plaats van
schuiven en vergelijken (één load, één 16-bits store, geen sprong), en pushen per
strook van 16 regels in plaats van per regel. Dat is vervolgens gebouwd en
gemeten, op dezelfde buffer vol ruis:

| | tijd |
|---|---|
| `pushSprite` van TFT_eSPI | 48.005 us |
| eigen lus: opzoektabel, stroken van 16 regels | **37.977 us** |
| alleen de expansie, zonder de bus | 3.421 us |

Tien milliseconden sneller, ofwel **26 fps in plaats van 21**. De optelsom klopt:
37,98 = 34,56 (bus met stroken) + 3,42 (expansie). Van de winst komt ongeveer de
helft uit de vorm van de lus (8,25 -> 3,42 ms) en de helft uit stroken in plaats
van losse regels.

**En daarmee wordt een palet gratis.** De expansie is bij een tabel niet meer dan
`*pDst++ = arLut[*pSrc++]`, en die tabel kan net zo goed 256 zelfgekozen kleuren
bevatten als het 3-3-2 raster. Precies dezelfde kosten per pixel. De reden om
voor RGB332 te kiezen was dat TFT_eSPI het kant en klaar had - maar dat kant en
klare is nu juist wat vervangen wordt. Paletted 8bpp, het oorspronkelijke idee,
is dus zowel mooier als even snel.

De busvloer van 31,6 ms (32 fps) haal je alleen met DMA, die de expansie van de
volgende strook laat overlappen met het uitzenden van de vorige. Dat scheelt nog
~6 ms.

## 2026-07-27 — MultiPanel: wat een bewegende sprite echt kost

Nieuw voorbeeld `CleanGUI_for_TFT_eSPI/examples/MultiPanel`: barbaren lopen over
een groene map, twee knoppen spawnen er een (transparant of dekkend), een derde
ruimt op, en bovenin staat de fps. Tot 20 per soort, vooraf aangemaakt en met
`setDrawable(false)` uit beeld tot ze spawnen.

### Het gat in moveTo dat dit blootlegde
`moveTo()` herstelde de vrijgekomen stroken via de root, maar tekende zichzelf op
de nieuwe plek *zonder* de root: *"whatever this widget covers, it covers"*. Dat
klopt niet voor een transparant plaatje — meer dan de helft van deze barbaar is
doorzichtig — en ook niet voor een panel met afgeronde hoeken. Zonder ingreep
laat een bewegende sprite een spoor van zichzelf achter.

Daarom `Widget::coversOwnRect()`, met `false` als veilige default en een
`Panel`-override die kijkt naar afgeronde hoeken en naar wat `needsBackground()`
al wist. `moveTo` kiest daarop het snelle pad of een `repaintRegion` door de
root. `TouchscreenButton` is beveld en dekkend, dus het schuivende toetsenbord
van Inputfield houdt het snelle pad.

### Gemeten (100 fps is de klokcap van de demo, geen plafond van het bord)

| sprites | transparant | dekkend |
|---|---|---|
| 4-5 | 53 fps (4,7 ms/sprite) | 99 fps (nog op de cap) |
| 10 | 16 fps (6,3 ms/sprite) | 39 fps (2,6 ms/sprite) |
| 20 | 3 fps (16,6 ms/sprite) | 16 fps (3,1 ms/sprite) |

Bij lage dichtheid klopt de raming van ~4 ms voor een transparante sprite. Maar
**transparant schaalt niet lineair en dekkend wel** — en dat is de echte les van
dit voorbeeld.

De reden: een transparante sprite laat zijn eigen rechthoek door de **hele tree**
opnieuw opbouwen. In die 64x64 zit niet alleen de map maar ook elke andere sprite
die er toevallig overheen valt, en die betaalt dan opnieuw zijn eigen runs. Met
20 sprites van 64x64 op een map van 320x172 beslaat het spriteoppervlak anderhalf
maal de map, dus elke verplaatsing tekent er gemiddeld nog een paar mee: kosten
die groeien met de *dichtheid*, niet met het aantal. Een dekkende sprite tekent
alleen zichzelf en blijft daarom netjes lineair; alleen zijn vrijgekomen stroken
gaan door de root, en die zijn dun.

De commandobuffer van `AsyncDisplay` stond bij alles boven de vijf sprites vol
(300). Dat is geen fout maar tegendruk: de tekentaak is de rem, de bus is de
bodem. Een grotere buffer maakt dus niets sneller.

### Waar de winst zit als dit ooit moet
Niet in sneller tekenen maar in minder samenstellen: de sprite met zijn
achtergrond in een klein DRAM-buffertje samenvoegen en die in één dekkende push
versturen. Dan valt zowel het per-run-tarief als het hertekenen van de buren weg.
Dat is een nieuw mechanisme in CleanGUI en dus een aparte ronde.

### Onderweg
`Widget<0>` liet gcc struikelen over `_arChildren[i]` op een array van lengte
nul (`-Werror=array-bounds`), zodra een `Panel<0>` als los object werd gebruikt.
Elke lus over de kinderen heeft nu `(MaxNofChildren > 0) &&` erbij; dat is een
template-parameter, dus de conditie valt bij het compileren weg.

### De stack van de AsyncDisplay-taak: gemeten, niet geraden
Elk voorbeeld gaf die taak
`20000 + CommandBufferSize*sizeof(DisplayCommand) + MaxNofClientTasks*sizeof(PrintContext)`
= 39.440 bytes. Met `dumpStackHighWaterMarkIfIncreased()` gemeten:

| example | belasting | stack over | gebruikt |
|---|---|---|---|
| MultiPanel | 20 sprites, transparante blits, clip-rects | 36.952 | **2.488** |
| Inputfield | free fonts, `drawStringOpaque` per toets, schuivend toetsenbord | 36.944 | **2.496** |

Twee heel verschillende commando-mengsels, 8 bytes uit elkaar. Logisch: die taak
haalt één commando uit de queue en voert het uit — er is geen diepe keten. De
watermark loopt vanaf taakstart, dus kalibratie en het mounten van LittleFS
zitten erin. Inputfield kon zonder touch gemeten worden dankzij
`CRT_INPUTFIELD_AUTOSLIDE` en `CRT_INPUTFIELD_AUTOTYPE`.

Beide termen in die formule telden geheugen mee dat niet op de stack staat: de
commandobuffer is een FreeRTOS-queue op de heap (`xQueueCreate`, `crt_Queue.h:29`)
en de `PrintContext`s zijn leden van `AsyncDisplay` (`sizeof` = 628). Alle zeven
voorbeelden staan nu op een kale `8000` — ruim drie keer de gemeten piek — en de
buffer ging van 300 naar 1000 commando's.

Per saldo: queue 19.200 -> 64.000 bytes heap, stack 39.440 -> 8.000. Netto ruim
13 KB erbij, van de ~200 KB die vrij was. `apps/pong_cleangui` heeft de oude
formule nog; dat is een app en geen voorbeeld, dus die is met rust gelaten.

Wat een grotere buffer *niet* doet: de beeldsnelheid verhogen. Zolang er meer
commando's worden geproduceerd dan de bus afvoert, loopt ook een buffer van 1000
vol — het duurt alleen langer. Wat hij koopt is speling bij pieken (spawnen, een
volledige paginatekening).

## 2026-07-27 — ImageViewer hertekent alleen nog wat verandert

### Wat er mis mee was
Elke plaatjeswissel tekende het hele scherm opnieuw: **twee** volledige
vullingen van 76.800 pixels, ~31 ms per stuk. De eerste kwam van
`backgroundPanel`, de tweede van `imagePanel` — dat was óók schermvullend, en een
panel dat groter is dan zijn dekkende plaatje vult eerst zijn `colFg`. Die
tweede overschilderde de eerste dus in dezelfde kleur. Plus caption en zes
knoppen: ~75 ms voor een wissel die soms maar 32x32 pixels verandert.

### De drie ingrepen
1. **`imagePanel` is nu precies zo groot als het plaatje** en wordt via zijn
   positie gecentreerd, niet via `Alignment::MidMid` binnen een schermvullend
   panel. Daarmee is `needsBackground()` onwaar en verdwijnt de tweede vulling.
2. **Alleen de veranderde rechthoek hertekenen.** `Widget::repaintBehind` is
   publiek geworden als `repaintRegion(topLeftPix, sizePix)` — de machinerie was
   er al voor het wegschuivende toetsenbord. De viewer onthoudt waar het plaatje
   staat en hertekent bij een wissel de vereniging van oude en nieuwe rechthoek.
   Omdat alles gecentreerd staat, is die vereniging simpelweg de grootste van de
   twee. De tree tekent zichzelf daarbinnen in de gewone volgorde, dus knoppen
   die eroverheen vallen komen vanzelf weer bovenop; de rij expliciete
   `show()`-aanroepen kon weg.
3. **De caption in een eigen regelhoog panel**, met `Text::setOpaque()`. Dat
   wist zijn eigen vak in dezelfde pass. Het panel is nodig omdat
   `Text::calcPaddingSizePix()` dat vak van de *ouder* afleidt: met de page als
   ouder zou het 240 px hoog worden en een strook van bijna het hele scherm
   blanken.

Voor het kruis kwam er `Widget::setDrawable(bool, bIncludeChildren)` bij, de
runtime-tegenhanger van het bestaande `isDrawable()`. Bij een gewoon plaatje
staat het kruis nu niet alleen uit voor touch maar ook uit de tekening, zonder
uit de tree te gaan. De strook waar het staat wordt alleen hertekend als die
zichtbaarheid *verandert* — en als aparte regio, want de bounding box van
plaatje plus kruis zou een brede band meeslepen die niets doet.

### Onderweg gevonden
`Widget::setSize()` gooide zijn `coordTypeSize`-parameter weg. Een widget die in
promillage was aangemaakt las een maat in pixels dus als promillage en werd
minuscuul. Nu wordt het veld gewoon toegekend. Geen bestaande aanroeper leunde
erop: `PageRoot` en `Text` gaven allebei al `Pixels` door aan een widget die dat
ook al was.

### Meten
`showCurrentImage()` logt nu zijn eigen duur in microseconden. De verwachting is
~1 ms voor een frame-stap binnen een sheet en ~10 ms voor de grootste wissel,
tegen ~75 ms daarvoor; dat moet het bord bevestigen.

## 2026-07-27 — Partitietabel zonder OTA

OTA is niet nodig, dus `otadata` en de tweede app-partitie zijn eruit en de app
heet weer `factory`. spiffs is teruggebracht tot 100 KB. Wat dat oplevert:

| | oud | nieuw |
|---|---|---|
| app | 0x140000 (1,25 MB) | 0x3D7000 (3,84 MB) |
| spiffs | 0x170000 (1,44 MB) | 0x19000 (100 KB) |
| otadata + app1 | 0x142000 | — |

De app zelf is 0x66400 (410 KB), dus daar zit nu 90% vrij. spiffs ligt achteraan
zodat de tabel op precies 0x400000 uitkomt. Het gat tussen 0xe000 en 0x10000
blijft: een app-partitie moet op een veelvoud van 0x10000 beginnen.

**Gevolg bij het flashen:** spiffs is verhuisd van 0x290000 naar 0x3E7000, dus
LittleFS vond daar de resten van de oude app1-partitie:

```
lfs.c:1075:error: Corrupted dir pair at {0x0, 0x1}
E esp_littlefs: mount failed, (-84)
I DisplayAdapter: Calibration file not found - forcing recalibration
```

Dat is precies zoals bedoeld - `LITTLEFS.begin()` formatteert dan zelf - maar de
touchkalibratie is er wel mee weg. Eenmalig de hoeken aantikken en hij staat er
weer, op de nieuwe plek.

## 2026-07-27 — Spritesheet-frames: wie bezit de actieve row/col?

### De vraag
De ImageViewer moest een kruis van vier knoppen krijgen om door de frames van een
spritesheet te lopen. Eerste ingeving: row en col erbij in `Image`. Dat klopt
niet, en waarom niet is het hele ontwerp:

**Een `Image` beschrijft de bitmap, niet een verschijning op het scherm.** Hij
heeft ook geen x en y, en om dezelfde reden. Bovendien is hij `constexpr` en
leeft hij in flash: een selectie die verandert kán er niet in staan. En twee
panels die hetzelfde sheet tonen willen elk hun eigen frame.

Dus: `nofRows`/`nofCols` (de indeling van de bitmap) blijven in `Image`, en
`_frameRow`/`_frameCol` (welk frame nú te zien is) horen in `Panel`.

### Wat er is bijgekomen
`Panel::setImageFrame(row, col)` / `clearImageFrame()` / `hasImageFrame()`.
Buiten bereik wordt omgeslagen, zodat doorstappen aan de aanroepkant geen
grenscontrole nodig heeft. `setImage()` laat de framekeuze vallen — die hoorde
bij het vorige plaatje.

De keuze reist mee door de tekenketen: `IDisplay::drawImage` heeft er
`frameRow, frameCol` bij gekregen (negatief = het hele plaatje), en daarmee ook
`AsyncDisplay` en `DisplayCommand`. Die laatste zet ze in zijn ongebruikte
`intB`/`uintA`-velden in plaats van er nieuwe bij te krijgen: het commando-buffer
telt 300 entries, dus elk extra veld kost hier ruim 2 KB DRAM.

### De blit zelf
Een frame is een rechthoek *binnen* de array, dus zijn rijen liggen niet naast
elkaar: elke volgende begint `image.width` pixels verderop. `pushImage` kent geen
source-stride, dus het gaat per scanlijn — één `pushImage` per regel. Dat kost een
`setWindow` (~34 us) per regel in plaats van één voor de hele blit: een 32x32
frame komt op ~1,4 ms waar de bus 0,4 ms nodig heeft.

Bewust niet geoptimaliseerd met een eigen `setWindow` + `pushPixels`: dan verlies
je de viewport-clipping die in `pushImage` zit, en 1 ms voor een sprite is geen
probleem. Als dit ooit wel knelt (veel sprites per frame), is dát de plek.

### In de viewer
Het kruis verschijnt alleen bij een sheet, en wordt dan ook alleen dan
*enabled* — een onzichtbare knop die nog wel luistert, slikt aanrakingen op die
voor iets anders bedoeld waren. De eerste druk op een van de vier zet
`row,col = 0,0` in plaats van weg te stappen van een frame dat nog niet gekozen
was; pas daarna loopt het kruis. Bij een andere afbeelding vervalt de keuze.

## 2026-07-27 — Image-assets in flash: converter-tools en het ImageViewer-voorbeeld

### Wat er nu staat
Een `tools`-map in de projectwortel met Python-gereedschap dat elk door Pillow
leesbaar beeld omzet naar een RGB565-header in flash, plus een nieuw voorbeeld
`CleanGUI_for_TFT_eSPI/examples/ImageViewer` dat die plaatjes met twee
touchknoppen doorbladert.

```
tools/img2rgb565.py      één plaatje omzetten
tools/allImg2rgb565.py   alles omzetten wat veranderd is
tools/venv_bootstrap.py  zorgt dat de venv gebruikt wordt zonder activeren
tools/setup_venv.sh      eenmalig: venv + requirements.txt
tools/README.md          het volledige verhaal
```

### De structuur die dit mogelijk maakt zonder CMake aan te raken
Bronnen staan onder een map `images`; de gegenereerde headers in
`images/_rgb565/`. Het pad onder `images` wordt platgeslagen in de outputnaam
(`level1/characters/speler.png` -> `level1_characters_speler.h`), zodat gelijke
bestandsnamen in verschillende submappen elkaar niet overschrijven.

De example-map staat één keer in `main/CMakeLists.txt`; alles daaronder wordt
met **relatieve** `#include`s bereikt. Er komt dus nooit een CMake-wijziging bij
per plaatje of submap. Bewijs dat dit klopt: het Inputfield-voorbeeld hergebruikt
de plaatjes van Panel via `#include "../Panel/images/_allImages.h"`, en die
`images`-map staat in geen enkele include-pad-lijst.

### Parameters in de bestandsnaam
`-trcolor_RRGGBB` en `-ssh_RxC`, achteraan de naam. Ze blijven in de headernaam
staan (uniek en zelfverklarend) maar verdwijnen uit de C++-namen; daar worden ze
constructor-argumenten. `Image` heeft daarvoor `nofRows`/`nofCols` gekregen
(default 1x1, voorlopig alleen opslag).

**Valkuil bij die constructors:** géén default-argumenten gebruiken. Met
defaults zou `Image(p, w, h, 0x00FF00FF)` stilzwijgend de spritesheet-overload
kiezen in plaats van de transparantie-overload, want een hexliteraal die in een
`int` past *is* een `int` en matcht `int32_t nofRows` exact, terwijl `uint32_t
colTransparent` een conversie vergt. Nu onderscheidt alleen het aantal
argumenten de overloads, en dat kan niet misgaan.

### `_allImages.h` wordt herbouwd uit de headers zelf
Elke gegenereerde header draagt een `// crt-img-meta:`-regel met naam, afmeting,
transparante kleur en sheet-indeling. `_allImages.h` wordt daaruit opgebouwd, dus
er is geen apart administratiebestand dat kan verouderen. Hij bevat ook
`arAllImages[]` / `arAllImageNames[]` / `nofAllImages`, waardoor de ImageViewer
geen enkel plaatje bij naam hoeft te kennen: `.png` erbij, tool draaien, staat
erin.

Het Panel-voorbeeld is op dezelfde leest geschoeid: `crt_TestPanelImages.h` is
gesplitst in `images/gradient.h`, `images/ring.h` en `images/_allImages.h`.

### Twee dingen die bij het testen boven kwamen
- **`venv/bin/python` is een symlink naar de systeem-python.** De eerste
  venv-detectie vergeleek `sys.executable` met `os.path.samefile()` en
  concludeerde dus altijd "we zitten er al in" — waarna `import PIL` faalde.
  `sys.prefix` is wél wat naar de venv verschuift, en is wat `site-packages`
  volgt.
- **De index moet altijd herbouwd worden, niet alleen na een conversie.** Een
  met de hand verwijderde header liet anders een `#include` van een niet-bestaand
  bestand achter in `_allImages.h`, en dat merk je pas veel later als
  compileerfout.

### Incrementeel omzetten
`allImg2rgb565_last_called.txt` (in de images-wortel) bevat epoch-seconden van
het moment waarop de laatste geslaagde run *begon* — niet waarop hij eindigde,
anders valt een bewerking tijdens de run precies in het gat en wordt hij nooit
opgepikt. Er wordt ook omgezet als de header simpelweg ontbreekt, want anders
zou een verse checkout of een weggegooide `_rgb565`-map niets regenereren.

## 2026-07-27 — Touch-polling vrat de helft van de tekentijd

### Aanleiding
Vraag: hoeveel tijd zou het schelen als elke toets een image in flash was, zodat
hij alleen nog geblit hoeft te worden in plaats van getekend? Om dat te kunnen
beantwoorden eerst gemeten waar de tijd van een keyboard-hertekening in gaat -
en daarbij bleek iets heel anders de hoofdrolspeler.

### Antwoord op de blit-vraag
Een volledige hertekening, uitgesplitst per commando (release-build):

| onderdeel | aantal | tijd | gemiddeld |
|---|---|---|---|
| rug (`fillBeveledRect`) | 1 | 16,5 ms | |
| toetsen (`fillBeveledRect`) | 31 | 17,8 ms | 574 us |
| labels (`drawString`) | 31 | 12,3 ms | 397 us |
| **totaal tekenwerk** | 64 | **47,1 ms** | |

Met een image per toets: 25.392 pixels blitten = 10,4 ms, en de labels zitten in
de image. Totaal 27,1 ms, dus **42% minder tekenwerk voor 51 KB flash**.

De rug levert vrijwel niets op (16,5 -> 15,7 ms): een egale vulling zit al op het
busplafond. Blitten helpt alleen waar je nu meer werk doet dan de pixels vragen,
en bij een toets is dat zo: 574 us voor 648 pixels is 886 ns/pixel tegen een
vloer van 409, doordat `fillBeveledRect` zeven losse `fillRect`s met elk een
eigen `setWindow` doet (~34 us overhead per stuk).

Het hele toetsenbord als **een** image zou 15,7 ms zijn - **67% minder** voor
76,8 KB - maar dan zitten de labels vast in het beeld en heeft elke
toetsenbordstand een eigen image nodig.

### De echte vondst: pollTouch
De wandkloktijd van diezelfde hertekening was 65-100 ms terwijl het tekenwerk
constant 43 ms was. Twee verkeerde verklaringen onderweg (queue-overhead; mijn
eigen meetlus die CPU zou stelen) - beide weerlegd door meting. Toen alles apart
geklokt:

```
leegdraaien 90089 us = tekenen 43603 + pollTouch 53495 (2x) + rest -7009
```

**`pollTouch` kostte ~25 ms per aanroep, elke 50 ms.** Losstaand gemeten kostte
een `getTouch()` 14 ms terwijl er niets werd aangeraakt.

### Waar die 14 ms in zat
Met tellers in `validTouch` gemeten: **18 debounce-rondes over 7 aanroepen**, dus
~2,6 rondes per `validTouch` - geen uitschieter. De factor die het deed was dat
`getTouch` `validTouch` **vijf keer** aanroept, onvoorwaardelijk:

| | |
|---|---|
| 5 x validTouch, elk ~2,6 debounce-rondes | ~13 rondes |
| elke ronde `delay(1)` | ~10 ms slapen |
| elke ronde `getTouchRawZ()` van 275 us | ~3,6 ms |

Die 275 us is inherent: de touch-SPI is bit-banged met twee
`delayMicroseconds(1)` per bit. En `delay(1)` slaapt, dus de tekentaak lag stil -
het was geen CPU-verbruik maar blokkade.

De vijf pogingen zijn overigens goed bedoeld: `validTouch` is streng (twee
positiemetingen die binnen `_RAWERR` moeten overeenkomen), en op een resistief
paneel wiebelt een echte vingerdruk. Vijf kansen waarvan er een hoeft te slagen
maakt het geheel ruisbestendig zonder echte drukken te missen. Alleen stopt de
lus nergens vroeg, dus het meest voorkomende geval - niemand raakt aan - betaalt
de volle prijs voor een vraag die na de eerste 275 us al beantwoord was.

### De reparatie
Twee regels in `TFT_eSPI::getTouch()`: een enkele Z-meting vooraf voor het
"geen druk"-geval, en `break` zodra een poging slaagt. Vastgelegd in
**`libs/CleanGUI_for_TFT_eSPI/TOUCH_PERFORMANCE.md`**, want het is een lokale
wijziging in TFT_eSPI die bij een upgrade verdwijnt.

| | voor | na |
|---|---|---|
| `getTouch()`, niets aangeraakt | 14.300 us | **245 us** |
| keyboard-hertekening, wandklok | 65-100 ms | **45,9 ms, stabiel** |

245 us is precies een `getTouchRawZ()`. De hertekening is nu net zo voorspelbaar
als het tekenwerk zelf; alle spreiding kwam van touch-polls die er wel of niet
tussen vielen.

**49% winst zonder een byte flash**, tegenover 42% van het tekenwerk voor 51 KB
aan images. De goedkope knop zat dus ergens anders dan de vraag suggereerde.

### Ook geprobeerd: meerdere commando's per waitAny
`AsyncDisplay` handelde er een per ronde af. Op 10 gezet: 95 -> 85 ms, ruim 10%.
Behouden, maar het was niet de kostenpost waarvoor ik het aanzag.

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

