# Migratiebrief: `apps/keukenweegschaal/weegcontrol.h`

2 call sites.

## Per call site

### regel 55 — `display.enable();`  *(risico: mechanisch)*

**Bedoeling:** Toestand WACHT_OP_AANKNOP_3SEC_INGEDRUKT: na wait(flagDrieSecondenKnop) is de aan-knop 3 seconden ingedrukt geweest, dus de weegschaal-display wordt aangezet en meteen daarna wordt de tarra gelezen. Dit is GEEN CleanGUI-widget: `display` is een pseudocode-object (klasse Display bestaat nergens in de repo) met een showString(x,y,string)-interface.

**Wordt:** Ongewijzigd laten. Raakt de CleanGUI-API niet; enable() hier is een eigen aan/uit van een weegschaal-display, niet crt::Widget::enable().

### regel 78 — `display.disable();`  *(risico: mechanisch)*

**Bedoeling:** Toestand DISPLAY, tak HasFired(flagDrieSecondenKnop): de gebruiker houdt de knop weer 3 seconden in, dus het apparaat gaat uit — display uit en terug naar WACHT_OP_AANKNOP_3SEC_INGEDRUKT. Geen CleanGUI-widget.

**Wordt:** Ongewijzigd laten. Raakt de CleanGUI-API niet.

## Observaties van de inventarisatie

Kernbevinding: geen van deze drie bestanden raakt CleanGUI. In alle drie samen komt geen enkele `show(`, `hide(`, `setDrawable(`, `isShown(`, `redraw(`, `redrawRegion(`, `repaintRegion(` of `paint(` voor — geverifieerd met een grep op de volledige bestanden; de enige treffers zijn de zeven hierboven gerapporteerde enable()/disable()-aanroepen op niet-widget-objecten. Er is dus ook nergens iets dat op het neveneffect show->enable of hide->disable kan leunen: dat neveneffect vereist een show/hide, en die staat hier niet. `apps/keukenweegschaal/weegcontrol.h` en `weegcontrol_obs.h` zijn ontwerp-schetsen in pseudocode, geen compileerbare C++ (`void losgelaten(knopNaam:string)`, `switch(state):` met dubbele punt, ontbrekende puntkomma's op regels 83/108/113 van _obs, en met `-` weggestreepte memberregels 7-12 van _obs); ze worden door geen enkele .ino geinclude en `apps/keukenweegschaal` staat niet in het include-pad van `main/CMakeLists.txt`. Het object `display` daarin heeft nergens in de repo een klassedefinitie, en zijn interface `showString(x,y,string)` bestaat niet in CleanGUI — ik kan daarom niet met absolute zekerheid zeggen wat de auteur met `Display` voor ogen had, maar er is geen aanwijzing dat het een CleanGUI-widget is. In `crt_GameControl.h` is `sceneDisplayControl` aantoonbaar een CleanRTOS-Task achter de interface `ISceneDisplayControl`; enable()/disable() zetten daar alleen Flags die de renderloop in `SceneDisplayControl::main()` (crt_SceneDisplayControl.h:181-199) tussen STATE_ENABLED en STATE_DISABLED laten schakelen, en dat renderen gaat naar de seriele console via `charMatrix.printMatrix()`, niet naar het scherm. Twee tekstuele bijna-treffers die géén migratie zijn: `display.showString(...)` op weegcontrol.h:65 en weegcontrol_obs.h:83 bevatten wel "show" maar matchen niet op `show(`. Ik heb geen bestaande workaround-regels gevonden (geen enable/disable pal na een show/hide, geen commentaar in de trant van "show zet de enable-toestand terug"). Ter context voor de bouw: de console-Pong wordt op dit moment niet gebouwd (`//#include <Pong2.ino>` op main/main.cpp:68), maar `apps/pong/pong` staat wel op het include-pad (main/CMakeLists.txt:135), dus het bestand moet blijven compileren. Conclusie voor de refactoring: deze groep vergt nul wijzigingen en levert nul risico op stil gedrag.

