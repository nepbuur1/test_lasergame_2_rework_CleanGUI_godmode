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
| 5 | `pong_cleangui_dg_adv` | bezig | |
| 6 | de 8 examples | te doen | |
| 7 | testharnas opnieuw — alles moet slagen | te doen | |
| 8 | `pong_cleangui_dg`, `pong_cleangui_cg`, `keukenweegschaal` | te doen | |
| 9 | `apps/pong/pong` | te doen | |
| 10 | `GameOverDisplayControl` opsplitsen in states | te doen | |

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
- **Eén bug is en passant gerepareerd** in `pdga_GameDisplayControl.h::showPage`.
  Daar stond `initBallAndPaddlePosition()` vóór `setDrawable(true, true)`, en
  die zette dus alle vijftig balslots, vijf crates en vijf vuurkorven weer aan
  die de `apply*States` net hadden uitgezet — waarna `show(true)` ze ook tekende.
  De volgorde is omgedraaid. Dit is een gedragswijziging, geen vertaling.

## Wat de gebruiker nog handmatig moet doen

- De eindtest op het bord. Pas daarna mag deze branch naar `main`.
