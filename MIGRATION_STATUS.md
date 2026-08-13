# Migratie: show / enable / draw uit elkaar

Spec: `devdocs/Claude_instructions/CleanGUI_show_enable_refactor.md`.
Branch: `show_drawable_enable_refactoring`, vertakt van `44551ce`.

Dit bestand is het geheugen van de run. Eén regel per stap, bijgewerkt zodra de
stap is gebouwd en getest. Word ik onderweg samengevat, dan zijn `git log` en
dit bestand genoeg om verder te kunnen.

## Stand

| # | stap | status | commit |
|---|---|---|---|
| 0 | branch + nulmeting | **gedaan** | dit |
| 1 | `readPaletteIndexAt()` op de PAL8-adapter | te doen | |
| 2 | testharnas `ShowEnableSemantics` tegen de huidige API | te doen | |
| 3 | `Widget` + `IWidget`: de nieuwe API | te doen | |
| 4 | `Panel`, `Text`, `TouchscreenKeyboard` | te doen | |
| 5 | `pong_cleangui_dg_adv` | te doen | |
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

## BESLUIT

*(ontwerpkeuzes die onderweg zijn genomen, zodat ze niet twee keer gemaakt
worden)*

## Wat de gebruiker nog handmatig moet doen

- De eindtest op het bord. Pas daarna mag deze branch naar `main`.
