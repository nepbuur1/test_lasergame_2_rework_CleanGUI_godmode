# Migratie: show / enable / draw uit elkaar

Spec: `devdocs/Claude_instructions/CleanGUI_show_enable_refactor.md`.
Branch: `show_drawable_enable_refactoring`, vertakt van `44551ce`.

Dit bestand is het geheugen van de run. Eén regel per stap, bijgewerkt zodra de
stap is gebouwd en getest. Word ik onderweg samengevat, dan zijn `git log` en
dit bestand genoeg om verder te kunnen.

## Stand

| # | stap | status | commit |
|---|---|---|---|
| 0 | branch + nulmeting | **gedaan** | `4dc67cc` |
| 1 | `readPaletteIndexAt()` op de PAL8-adapter | **gedaan** | `ad08c5e` |
| 2 | testharnas `ShowEnableSemantics` tegen de huidige API | **gedaan** | dit |
| 3 | `Widget` + `IWidget`: de nieuwe API | **gedaan** | `06b3f9e` |
| 4 | `Panel`, `Text`, `TouchscreenKeyboard` | **gedaan** | `32f94b8` |
| 5 | `pong_cleangui_dg_adv` | **gedaan** | `6e5fa97` |
| 6 | de 8 examples | **gedaan** | `74b30f2` |
| 7 | testharnas opnieuw — alles moet slagen | **gedaan, GROEN** | `fc9fb78` |
| 8 | `pong_cleangui_dg`, `pong_cleangui_cg`, `keukenweegschaal` | **gedaan** | `27a0e09` |
| 9 | `apps/pong/pong` | **gedaan** (geen wijziging nodig) | `27a0e09` |
| 10 | `GameOverDisplayControl` opsplitsen in states | **gedaan** | `ef7fb42` |

**Alle stappen zijn af.** Alle dertien doelen zijn gebouwd, geflasht en
opgestart zonder crash of watchdog: de acht examples, de drie pongvarianten met
touchscreen, de console-pong, en het testharnas. Het harnas staat op 37 van de
37.

Wat er nog moet gebeuren: de handmatige eindtest (zie onderaan). Daarna kan
deze branch naar `main`.

## Nulmeting (stap 0)

- `idf.py build` groen op `main` + lege branch. `build/app-template.bin` =
  555.936 bytes. Actieve app in `main/main.cpp`: `Pong_CleanGUI_dg_adv.ino`.
- Alleen bestaande waarschuwingen (`-Wvolatile` in `crt_AsyncDisplay.h`), geen
  nieuwe.
- Omgeving: `. ~/esp54/esp-idf/export.sh` (ESP-IDF v5.4.3), bord op
  `/dev/ttyUSB0`, gebruiker zit in `dialout`.
- Call sites geteld met het commando uit §3 van de spec: **182**, verspreid
  over **25 bestanden**.

## Inventarisatie

`devdocs/refactor_notes/migratiekaart.md` — per bestand: regel, huidige
aanroep, nieuwe aanroep, risico. Plus de lijst *leunt op neveneffect*: de
plekken waar een expliciete `enable()`/`disable()` bij moet omdat `show`/`hide`
dat niet meer doen. De ruwe data staat in `inventaris_raw.json`.

## Uitslag van het testharnas (stap 2)

Voorbeeld `ShowEnableSemantics`, gedraaid op het bord:

```
37 controles: 30 geslaagd, 7 gefaald (7 daarvan verwacht), 0 onverwacht geslaagd
EINDOORDEEL: VERWACHT ROOD - 7 controles falen zoals voorspeld; dat is de bug
```

Na stap 3 en 4 staat hetzelfde harnas op **37 van de 37**, met de zeven
hieronder als "OK (!)" — geslaagd terwijl hun vlag nog op *stuk voor de
refactoring* staat. Stap 7 zet die vlaggen om.

De zeven die faalden, en die nu slagen:

1. tekenbaar: en kapt zijn kinderen mee af
2. tekenbaar: ook de knop twee niveaus lager
3. assen: hertekenen laat uitgeschakeld uitgeschakeld
4. assen: en die knop meldt dus nog steeds niets
5. assen: show() laat een uitgeschakelde pagina uitgeschakeld
6. assen: hide() laat een ingeschakelde widget ingeschakeld
7. hide: en er staat wat eronder zat, niet zijn eigen colBg

Draaien: `python3 tools/select_app.py ShowEnableSemantics && idf.py build &&
idf.py -p /dev/ttyUSB0 flash && python3 tools/serial_capture.py --seconds 60
--until "EINDE TEST"`.

## BESLUIT

*(ontwerpkeuzes die onderweg zijn genomen, zodat ze niet twee keer gemaakt
worden)*

- **Het harnas compileert tegen beide API's.** `cgRedraw` / `cgMakeDrawable` /
  `cgMakeNonDrawable` in `crt_TestShowEnableSemantics.h` wijzen naar `show` +
  `setDrawable` zolang `CLEANGUI_HAS_DRAW_ENABLE_SPLIT` niet bestaat, en
  daarna naar `redraw` + `makeDrawable`. Zo hoeft het testbestand tussen stap
  2 en stap 7 niet te veranderen: de bibliotheek eronder verandert, en de
  asserties gaan daardoor van rood naar groen. Dat is de meting.
- **Twee helpers extra bij `readPaletteIndexAt()`** (stap 1):
  `paletteIndexForColour()` en `getIndexTransparent()`. Zonder de eerste moet
  een test tegen een hardgecodeerd getal vergelijken en test hij het palet mee
  in plaats van wie er getekend heeft.
- **`tools/select_app.py`** zet de actieve `#include` in `main/main.cpp` om.
  De stappen 5 t/m 9 doen dat een stuk of dertien keer; met de hand laat je een
  keer twee regels aan staan en dan zoek je de dubbele symbolen in de verkeerde
  hoek. **`tools/serial_capture.py`** leest de seriële uitvoer met een
  tijdslimiet in plaats van tot een toetsaanslag, wat `idf.py monitor` niet kan.
- **`page.show(true)` is in de nieuwe API niet meer hetzelfde.** Hij zet
  `_bDrawable` op de héle boom. In `pong_cleangui_dg_adv` staan 50 balpanels, 5
  crates, 5 vuurkorven en het GAME OVER-vignet bewust op niet-tekenbaar; die
  zouden er allemaal weer bijkomen. Daar moet dus `page.redraw(true)` staan.
  Zie de migratiekaart.
- **`hide()` tekent voortaan wél.** Drie apps zetten met opzet
  `setDrawable(false, true)` vóór `hide(true)` zodat het verbergen niets in de
  gedeelde commandobuffer schuift. Voor die plekken is de vertaling
  `makeNonDrawable(true) + disable(true)` en verdwijnt de `hide()` helemaal.
- **`disable()` hoort vóór `hide()`, niet erna.** De oude `hide()` disabelde
  als eerste en tekende daarna pas; de nieuwe tekent, en dat gaat via de
  commandobuffer. In dat venster komt de touch-taak nog langs, dus een widget
  die al van het scherm is kan nog een druk melden. Gevonden door de
  tegenspreker op `crt_TestTouchscreenKeyboard.h`.
- **`isShown()` is in `crt_TestTouchscreenKeyboard.h` `isDrawable()` geworden**,
  niet een eigen `bool` zoals de spec voorstelde. Dat kan omdat `hide()` en
  `show()` daar precies de tekenbaarheid omzetten, en het scheelt een veld dat
  uit de pas kan lopen — wat nou juist het bezwaar tegen `_bVisible` was.
- **`setDrawableTo`/`setEnabledTo` in `pdga_GameOverDisplayControl.h` heten nu
  `makeDrawableIf`/`enableIf`.** Met de oude naam bleef `grep -rn setDrawable`
  dit bestand aanwijzen, en dat is uitgerekend het commando waarmee de
  voortgang van deze migratie wordt afgevinkt.
- **`keukenweegschaal` en `apps/pong/pong` hoeven niets.** De inventarisatie
  telde daar zeven call sites, maar dat zijn heel andere functies: in
  `weegcontrol.h` is `display` een OLED met een eigen `enable()`/`disable()`
  zonder argument, en in `apps/pong/pong/crt_GameControl.h` gaat het om
  `ISceneDisplayControl`. Geen van beide is een CleanGUI-widget. Een
  zoek-en-vervang op `.enable(` was hier overheen gewalst.
  (`weegcontrol_obs.h` is bovendien dood: nergens geïncludeerd, en er staat een
  ontbrekende puntkomma in die nooit is opgevallen. Met rust gelaten.)
- **Eén bug is en passant gerepareerd** in `pdga_GameDisplayControl.h::showPage`.
  Daar stond `initBallAndPaddlePosition()` vóór `setDrawable(true, true)`, en
  die zette dus alle vijftig balslots, vijf crates en vijf vuurkorven weer aan
  die de `apply*States` net hadden uitgezet — waarna `show(true)` ze ook tekende.
  De volgorde is omgedraaid. Dit is een gedragswijziging, geen vertaling.

## Wat er wel en niet is aangeraakt, en waarom

**Aangepast — 9 doelen, 13 bestanden.**

| doel | bestanden |
|---|---|
| `pong_cleangui_dg_adv` | `pdga_GameDisplayControl.h`, `pdga_GameOverDisplayControl.h`, `pdga_StartPageDisplayControl.h` |
| `pong_cleangui_dg` | `pdg_GameDisplayControl.h`, `pdg_GameOverDisplayControl.h`, `pdg_StartPageDisplayControl.h` |
| `pong_cleangui_cg` | `pcg_DisplayControl.h` |
| examples | `AsyncDisplay`, `ImageViewer`, `Inputfield`, `MultiPanel`, `Panel`, `TouchscreenKeyboard` |

Plus het nieuwe example `ShowEnableSemantics`.

**Gebruiken de aangepaste functies wél, maar hoefden niet mee** — twee examples,
elk met één `page1.show(true)` aan het eind van de boomopbouw:
`TouchscreenButton` (r.154) en `TouchscreenButtonGroup` (r.222). Die aanroep
betekent nu `makeDrawable(true) + redraw(true)`; in die twee bomen staat niets
op niet-tekenbaar, dus de `makeDrawable` verandert niets, en het verdwenen
enable-neveneffect werd er nooit gebruikt — er wordt nergens iets uitgeschakeld
en `_bEnabled` staat vanaf de constructor op `true`. Beide gebouwd en geflasht
ter bevestiging.

**Lijken mee te moeten, maar zijn geen CleanGUI-widgets.** Een grep op
`.enable(` / `.disable(` raakt nog zes bestanden. Ze landen geen van alle op een
widget, en dat is aan de aanroep zelf te zien: `IWidget::enable` neemt altijd een
`bIncludeChildren`, en deze zijn argumentloos.

| bestand | waar het op landt |
|---|---|
| `pdga_GameControl.h`, `pdg_GameControl.h`, `pcg_GameControl.h` | `PaddleControl` — een `Task` met `IButtonListener` |
| `apps/pong/pong/crt_GameControl.h` | `ISceneDisplayControl` |
| `apps/keukenweegschaal/weegcontrol.h` en `weegcontrol_obs.h` | een OLED-display (`showString`) |

`TouchCalibrateAll` raakt de widgetboom helemaal niet aan — geen `PageRoot`,
geen `Panel` — en valt dus buiten al deze lijsten. Wel gebouwd (groen); niet
geflasht, want dat voorbeeld hérkalibreert alle vier de rotaties en vraagt om
zestien hoektikken.

## Wat de gebruiker nog handmatig moet doen

Alles wat een vinger op het scherm vraagt. Het harnas dekt de semantiek van
`show`/`enable`/`redraw` en de pixels, maar niet of de apps prettig spelen.
Op volgorde van hoe waarschijnlijk het is dat er iets is blijven liggen:

1. **`pong_cleangui_dg_adv`, het eerste beeld van een nieuw spel.** Daar is een
   volgordebug gerepareerd (zie BESLUIT hierboven): eerder werden alle vijftig
   balslots en de crates weer aangezet en getekend. Kijk of het veldmidden en
   de linkerbovenhoek schoon zijn bij het starten.
2. **`pong_cleangui_dg_adv`, de game-over-pagina.** Stap 10 heeft die
   herschreven van één wachtlus naar drie toestanden. Loop de hele route af:
   spel afgelopen zonder highscore → lijst → PLAY AGAIN; en met highscore →
   naam invoeren → toetsenbord omhoog → tikken → ENTER → lijst → PLAY AGAIN.
   Let op de fakkels: die horen in de lijstfase te branden en in de naamfase
   stil te staan.
3. **`pong_cleangui_cg`, de peddel.** De tak die de peddel toont heeft een
   nieuwe test gekregen (`gamePanel.isDrawable()`). Controleer dat de peddel
   verschijnt bij het starten en niet blijft staan of over de startpagina komt.
4. **De drie pongvarianten, de peddelknoppen.** Elke `enable()` die vroeger
   overbodig was is nu dragend. Een knop die niet reageert of ingedrukt blijft
   staan wijst op een gemiste `enable()`/`disable()`.
5. **`TouchscreenKeyboard` en `Inputfield`.** Toetsenbord verbergen door
   ernaast te tikken, en weer terug.

Daarna kan deze branch naar `main`.

## Het testharnas opnieuw draaien

```
python3 tools/select_app.py ShowEnableSemantics
idf.py build && idf.py -p /dev/ttyUSB0 flash
python3 tools/serial_capture.py --seconds 60 --until "EINDE TEST"
```

De laatste regel is het oordeel. `GROEN` betekent: geen enkele verrassing.
