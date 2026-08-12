# Task 2 — CleanGUI: drie orthogonale assen (show / enable / draw)

> Opdracht voor Claude. Zoals in `Claude_instructions.md` staat: pas uitvoeren na
> expliciet verzoek. Dit bestand is de **specificatie**; leun er op in plaats van
> op je geheugen of op een samenvatting van een eerdere sessie.
>
> Opgesteld 2026-08-12, na een eerste nachtelijke poging die bewust is
> teruggedraaid. Zie *Voorgeschiedenis* onderaan.

---

## 1. Het probleem

In CleanGUI doet `show(bIncludeChildren)` op dit moment drie dingen tegelijk:
het zet een zichtbaarheidsvlag, het **tekent**, en het **enablet** de hele
subboom. Daardoor is er geen manier om te hertekenen zonder je enable-toestand
kwijt te raken.

De keten die dat afdwingt loopt zo:

1. `Widget::redrawRegion()` keert vroegtijdig terug op `!_bVisible`;
2. → je kunt hem dus niet gebruiken voor de *eerste* keer tekenen;
3. → dus moet je wel `show()` aanroepen;
4. → en `show()` zet `_bVisible` **én** enablet alles;
5. → dus wordt je enable-toestand bij elke hertekening overschreven.

Drie apps werken daar met commentaar en al omheen:
`pdga_GameDisplayControl.h` (r.741-742), `pdg_GameDisplayControl.h` (r.316) en
`pcg_DisplayControl.h` (r.474). Het concrete symptoom dat dit veroorzaakte: een
knop die zijn eigen pagina wegzet blijft voorgoed verzonken staan, omdat
`show()` → `enable(false)` de lopende druk wegpoetst waardoor `touchReleased()`
geen bericht meer stuurt.

**Doel:** drie assen die elkaar niet raken — *hoor ik bij het beeld*, *ben ik
aanraakbaar*, *teken nu* — en daarna alle afhankelijke apps, tests en examples
op dat nieuwe stramien zetten.

---

## 2. De nieuwe API

| | nu | straks |
|---|---|---|
| drawable | `setDrawable(bool, bIncludeChildren)` | `makeDrawable(bIncludeChildren)` / `makeNonDrawable(bIncludeChildren)` |
| zichtbaar | `show`/`hide` zetten `_bVisible` **én** enablen/disablen | `show` = `makeDrawable` + `redraw`; `hide` = `makeNonDrawable` + `repaintRegion` |
| aanraakbaar | `enable`/`disable`, maar overschreven door elke `show` | `enable`/`disable`, ongemoeid |
| tekenen | zat vast aan `show(true)` | `redraw(bIncludeChildren)` — nieuw, raakt geen enkele vlag |
| `_bVisible` / `isShown()` | logboek van het laatste show/hide-commando | **verdwijnt**; `_bDrawable` bedient alle drie de gebruikers beter |

Samengevat als code:

```cpp
makeDrawable(bIncludeChildren)      // vlag: ik hoor bij het beeld. Tekent niets.
makeNonDrawable(bIncludeChildren)   // vlag: ik hoor er niet bij. Wist niets.

redraw(bIncludeChildren)            // teken nu. Raakt geen enkele vlag.

show(bIncludeChildren)              // == makeDrawable + redraw
hide(bIncludeChildren)              // == makeNonDrawable + repaintRegion(mijn rect)

enable(bIncludeChildren)            // de andere as, ongemoeid
disable(bIncludeChildren)
```

### 2.1 Waarom `redraw` en niet `repaint`

Er bestaan al drie tekenmethodes, en hun namen volgen een patroon dat je moet
respecteren: **`redraw…` begint bij mij, `repaint…` begint bij de wortel.**

| naam | begint bij | oppervlak | clipt? |
|---|---|---|---|
| `paint()` | mij | alleen ik | nee |
| `redraw(bIncludeChildren)` **← nieuw** | mij | mijn hele vlak | nee |
| `redrawRegion(pos, size)` | mij | die regio | **nee** ⚠️ |
| `repaintRegion(pos, size)` | **de wortel** | die regio | **ja** |

`repaintRegion` is niet "nog een manier om alles te tekenen", het is de enige
veilige manier om een *stuk* te tekenen: het knipt de regio af op de root, zet
`setClipRect` aan, tekent vanaf de wortel en zet de clip weer uit.
`redrawRegion` is de recursie-primitive en clipt zelf niet — een widget die de
regio half raakt tekent gewoon zijn hele vlak.

Daarom moet `hide()` per se `repaintRegion` gebruiken: wat er **onder** je zat
moet terugkomen, en dat zijn je kinderen niet.

### 2.2 `_bVisible` mag weg — en dat verwijdert code

`_bVisible` pretendeert "er staat nu iets van mij op het scherm", maar is
feitelijk *een logboek van het laatste commando uit het show/hide-paar*. Het
loopt op twee manieren uit de pas:

1. `show()` op een niet-drawable widget zet hem op `true` terwijl er niets
   getekend is — en `page.show(true)` loopt door de hele boom;
2. `makeNonDrawable()` + een hertekening wist je pixels terwijl de vlag op
   `true` blijft staan.

De drie gebruikers, en wat ervoor in de plaats komt:

| plek | vervanging | gevolg |
|---|---|---|
| `Widget::redrawRegion` r.357 | `_bDrawable` | **winst.** De guard vuurt nu nooit (die ene `page.show(true)` zet alles op `true`), dus niet-drawable subbomen worden volledig doorlopen. Met `_bDrawable` wordt de subboom bij de wortel afgekapt |
| `Widget::moveTo` r.394 | `_bDrawable` | gelijk |
| `Panel::hide` r.226, `Text::hide` r.196 | de guard verdwijnt met het probleem: `hide()` wordt `makeNonDrawable` + `repaintRegion` | altijd correct, ongeacht of er ooit iets van je stond |

**Wat er dus uit kan:** `_bVisible`, `isShown()` uit `IWidget` en `Widget`,
`Panel::show`, `Panel::hide`, `Text::show`, `Text::hide`, `hidePanel()`,
`hideText()`. `show`/`hide` hoeven niet meer virtueel te zijn. `showPanel()`
blijft, als `Panel::paint()`.

Het ene example dat `isShown()` gebruikt
(`crt_TestTouchscreenKeyboard.h`) houdt voortaan zijn eigen `bool` bij.

> **Alternatief dat is overwogen en afgevallen:** `_bVisible` privé maken en
> hernoemen naar `_bPainted` ("ik heb zelf getekend en niet zelf gewist"),
> bijgehouden in `showPanel()`/`hidePanel()`. Dat is eerlijker dan nu, maar
> zodra `hide()` op `repaintRegion` overgaat heeft niemand het bit meer nodig.
> Bouw het niet; kom je onderweg een gebruiker tegen die het écht nodig heeft,
> noteer dat dan onder **BESLUIT** in het statusbestand.

### 2.3 Waarom `makeDrawable`/`makeNonDrawable` en niet `setDrawable(bool,…)`

Dit is de enige wijziging in het hele plan **die de compiler laat meepraten**.
Twee argumenten worden er één, dus elke call site breekt. En omdat deze apps die
dingen als drieluik schrijven —

```cpp
page.setDrawable(false, true);  page.hide(true);  page.disable(true);
```

— zet de compilerfout op regel 1 je meteen met je neus op regel 2 en 3, waar de
stille gedragswijziging zit. **Doe de hernoeming daarom in dezelfde pass als het
ontkoppelen**, juist omdat hij luid is.

### 2.4 Wat je niet per ongeluk moet vereenvoudigen

`redraw(bIncludeChildren)` mag ongeclipt zijn, net als `show(true)` nu is —
gedragsidentiek, geen nieuw risico. Buiten het scherm tekenen is in deze apps
routine (`initKeyboardPos()` parkeert het toetsenbord op `y = screenHeight`) en
kan niet misgaan: de adapter heeft **altijd** een viewport, en elke TFT_eSPI-
primitive begint met `if (_vpOoB) return;` plus clipping.

Maar: `applyViewport()` in de adapter **snijdt** de eigen viewport van een
widget met de actieve clip in plaats van hem te overschrijven. Zonder die
doorsnede zou een `drawImage` of `drawStringOpaque` binnen een partiële update
stilletjes buiten de regio tekenen. Laat dat staan.

---

## 3. Het stappenplan

Werk de stappen **één voor één** af, met een commit per stap.

| # | stap | omvang | verificatie |
|---|---|---|---|
| 0 | branch + nulmeting | — | `idf.py build` |
| 1 | pixel-accessor `readPaletteIndexAt()` op de PAL8-adapter | 1 bestand | build |
| 2 | testharnas `ShowEnableSemantics` tegen de **huidige** API | nieuw example | sommige asserties **moeten falen** — dat is het bewijs van de bug |
| 3 | `Widget` + `IWidget`: de nieuwe API | 2 bestanden | build breekt overal (bedoeld) |
| 4 | `Panel`, `Text`, `TouchscreenKeyboard` | 3 | build |
| 5 | `pong_cleangui_dg_adv` | 4 bestanden | build + flash + monitor |
| 6 | de 8 examples in `libs/CleanGUI_for_TFT_eSPI/examples` | 8 | per stuk build + flash |
| 7 | testharnas opnieuw | — | alles moet nu slagen |
| 8 | `pong_cleangui_dg`, `pong_cleangui_cg`, `keukenweegschaal` | 8 | build + flash |
| 9 | `apps/pong/pong` | 1 | build + flash; staat gewoon in het includepad (zie §6) |
| 10 | `GameOverDisplayControl` opsplitsen in expliciete states | 1 | build + gerichte tests |

**Stap 2 vóór stap 3 is bewust:** eerst vastleggen wat er nú stuk is.

Tel de call sites zelf opnieuw in plaats van een getal uit dit document te
geloven:

```bash
grep -rn "setDrawable\|isShown\|\.show(\|\.hide(\|\.enable(\|\.disable(" \
  --include='*.h' --include='*.ino' apps libs/CleanGUI libs/CleanGUI_async \
  libs/CleanGUI_for_TFT_eSPI | wc -l
```

Stap 10 is los van de rest te doen: in `GameOverDisplayControl` wordt binnen één
`while(true)`-lus op álle flags tegelijk gewacht. Duidelijker zijn meerdere
logische states die in elkaar overgaan, waarbij per state alleen op de flags
wordt gewacht die daar van toepassing zijn.

---

## 4. Zelfstandig testen — dit kan wél

Twee dingen maken dit plan grotendeels autonoom uitvoerbaar, zonder dat er
iemand naar het scherm hoeft te kijken.

**Aanrakingen zijn na te maken.** `ITouchListener` is de complete grens tussen
hardware en widgets, en `TouchscreenButton::touchPressed(pos)` is publiek. De
touch-detector doet letterlijk niets anders dan die aanroep. Testcode die

```cpp
btn.touchPressed(p);
btn.touchReleased(p);
```

doet loopt dus exact hetzelfde pad, en de callback komt synchroon terug. De repo
heeft dat patroon al: `CRT_INPUTFIELD_AUTOTYPE` — *"keyPressed is the same entry
point the keyboard uses"*.

**Pixels zijn terug te lezen.** De PAL8-adapter tekent in een schermvullende
8-bits sprite in DRAM waarin elke byte een palette-index is; `present()` leest
die alleen maar uit. `AsyncDisplay::waitUntilDrawn()` is de barrière. Eén
accessor van twee regels (stap 1) en je kunt hard asserteren of een widget
getekend is of niet.

Daarmee is precies de bug die we fixen automatisch te betrappen: *pixel is
achtergrond* **én** *touch levert nul callbacks*.

| wel autonoom | niet autonoom |
|---|---|
| alle enable/disable-gating | hoe het eruitziet |
| `redraw` clobbert de enable-toestand niet | of een animatie soepel oogt |
| groep-gating, ook groep-in-groep | de echte touch-hardware (drempels) |
| `bPressed` na disable→enable | of het spel lekker speelt |
| wel/niet getekend, per pixel | `pong_cleangui_dg` en `_cg`: die gebruiken de directe RGB565-adapter, dus daar geen pixeltests |
| compileren van alle 13 doelen | |

---

## 5. Werkwijze — niet uit je context lopen

- **Dit bestand is de specificatie**, niet je geheugen.
- Eén **commit per stap**, met een regel per stap in een
  `MIGRATION_STATUS.md` in de repo-root van de branch.
- Word je onderweg samengevat, dan zijn `git log` en dat statusbestand genoeg om
  verder te kunnen.
- De 21 migratiebestanden van stap 5, 6 en 8 zijn disjunct en kunnen parallel;
  de bouw- en flashstappen zijn serieel (één `main.cpp`, één bord).
- Kom je een ontwerpdetail tegen dat het oordeel van de gebruiker vraagt: neem
  een besluit, zet het onder **BESLUIT** in het statusbestand, en ga door met de
  rest. Vastlopen op één vraag kost de hele run.

---

## 6. Randvoorwaarden en valkuilen

- **Draai nooit `idf.py erase_flash`.** De touchkalibratie leeft in de
  spiffs-partitie uit `partitions.csv`; wissen betekent opnieuw kalibreren.
- **De kalibratie is sinds 2026-08-12 per schermrotatie opgeslagen** (alle vier
  naast elkaar in `/TouchCalData1`). Apps en examples met verschillende
  rotaties kunnen dus achter elkaar geflasht worden zonder dat er om kalibratie
  wordt gevraagd — mits het voorbeeld `TouchCalibrateAll` één keer is gedraaid.
  Is dat op deze machine/dit bord nog niet gebeurd, doe dat dan eerst; het kost
  vier keer vier hoeken aantikken en daarna nooit meer.
- **`.claudeignore` is hiervoor al aangepast** (2026-08-12): `apps` stond erop
  met de opmerking "voor nu even niet nodig", terwijl de stappen 5, 8, 9 en 10
  juist in `apps/` zitten. Alleen `apps/*.zip` blijft eruit. Kom je toch een app
  tegen die je niet kunt lezen, kijk dan eerst of die regel is teruggekomen.
- **`apps/pong/pong` botst niet** — die CMakeLists-swap is niet meer nodig.
  In `main/CMakeLists.txt` stond dat die app op headernamen zou botsen omdat hij
  nog de kale `crt_`-namen gebruikt. Dat is nagegaan op 2026-08-12 en klopt niet:
  zijn vijf headers komen nergens anders in de boom voor, hun klassen ook niet,
  en de drie CleanGUI-varianten hebben elk hun eigen prefix én namespace.
  `"../apps/pong/pong"` staat sindsdien gewoon aan; beide bouwrichtingen zijn
  geverifieerd. Stap 9 heeft dus geen bijzondere behandeling nodig.
- Bouwen en flashen: zie §7.

---

## 7. Omgeving

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

`idf.py` staat op de machine van de auteur niet in PATH; eerst sourcen:

```bash
. ~/esp54/esp-idf/export.sh          # ESP-IDF v5.4.3, toolchain in ~/.espressif
```

Op een andere machine kan dat pad afwijken — de standaardlocaties zijn
`~/esp/esp-idf` en `/opt/esp-idf`. Zoek hem op in plaats van te gokken.

`sdkconfig` staat in `.gitignore` en wordt opnieuw gegenereerd zodra hij
ontbreekt (verse checkout, WSL-herstart). Alles wat moet afwijken van de
IDF-defaults staat daarom in het wél gecommitte **`sdkconfig.defaults`**:
`CONFIG_FREERTOS_HZ=1000` (anders faalt cmake meteen op arduino-esp32), de eigen
`partitions.csv`, en 4MB flash. Zet nieuwe niet-default instellingen daar bij,
anders zijn ze weg bij de volgende regeneratie.

Welke app of welk voorbeeld gebouwd wordt, kies je met het `#include` van de
`.ino` in `main/main.cpp`. Precies één regel mag actief zijn.

Verder: `CLAUDE.md` in de repo-root bevat de hardware- en TFT_eSPI-kennis
(SPI-plafond, sprite-kosten, DMA-omslagpunt). Lees die eerst.

---

## 8. Voorgeschiedenis

Op 2026-08-11 is dit plan 's nachts voor het eerst uitgevoerd, op de branch
`refactor/cleangui-draw-enable-split`. Die poging kwam tot en met stap 3 plus
een deel van stap 5, en is op 2026-08-12 op verzoek volledig teruggedraaid: de
branch is verwijderd en `main` staat weer op `13f7254`.

Het teruggedraaide werk is niet weg maar staat lokaal op de machine van de
auteur onder de tag **`backup/nachtwerk-2026-08-11`** (3 commits plus het
werk dat nog niet gecommit was). Die tag is niet gepusht; wil je hem elders
bekijken, push hem dan eerst.

Directe aanleiding om terug te draaien: de poging bleek niet zelfstandig af te
maken doordat apps en examples verschillende schermrotaties gebruiken, wat elke
flash een nieuwe touchkalibratie uitlokte. Dat is nu opgelost — zie §6.
