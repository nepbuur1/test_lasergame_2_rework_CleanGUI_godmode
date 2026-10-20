# Migratiebrief: `apps/pong/pong/crt_GameControl.h`

2 call sites.

## Per call site

### regel 127 — `sceneDisplayControl.enable();`  *(risico: mechanisch)*

**Bedoeling:** STATE_KLIK_OM_TE_STARTEN: na een knopdruk wordt de bal gestart en daarna het scene-renderen aangezet, waarna de toestand naar STATE_GAME_RUNNING gaat. `sceneDisplayControl` is een `ISceneDisplayControl&` (crt_SceneDisplayControl.h:22-31); enable() zet daar alleen `flagEnable` (crt_SceneDisplayControl.h:92-95) van een CleanRTOS-Task die een ASCII-CharMatrix naar de console print. Geen widget, geen TFT.

**Wordt:** Ongewijzigd laten. Dit is Task-scheduling via een Flag, niet crt::Widget::enable(); de nieuwe show/hide/makeDrawable-API raakt dit niet.

### regel 138 — `sceneDisplayControl.disable();`  *(risico: mechanisch)*

**Bedoeling:** STATE_GAME_OVER: eerst het renderen stilleggen, daarna vTaskDelay(200) met het commentaar "wacht op evt parallel afrenderen van scene" — dus expliciet wachten tot de renderende task zijn laatste frame af heeft voordat de Game-Over-ASCII-art wordt gelogd. disable() zet alleen `flagDisable` (crt_SceneDisplayControl.h:97-100). Geen widget.

**Wordt:** Ongewijzigd laten. Dit is Task-scheduling via een Flag, niet crt::Widget::disable().

