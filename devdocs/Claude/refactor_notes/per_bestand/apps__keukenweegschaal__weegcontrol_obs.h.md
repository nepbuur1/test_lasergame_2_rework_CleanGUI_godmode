# Migratiebrief: `apps/keukenweegschaal/weegcontrol_obs.h`

3 call sites.

## Per call site

### regel 20 — `display.disable();`  *(risico: mechanisch)*

**Bedoeling:** In de constructor van WeegControl: het display start uit, want de weegschaal wacht eerst op 3 seconden aan-knop. Geen CleanGUI-widget; er staat ook geen enkele show/hide in dit bestand, dus dit is geen leunen op een neveneffect.

**Wordt:** Ongewijzigd laten. Raakt de CleanGUI-API niet.

### regel 71 — `display.enable();`  *(risico: mechanisch)*

**Bedoeling:** Toestand WACHT_OP_AANKNOP_3SEC_INGEDRUKT: na wait(flagDrieSecKnop) display aan en tarra lezen. Spiegelbeeld van weegcontrol.h:55. Geen CleanGUI-widget.

**Wordt:** Ongewijzigd laten. Raakt de CleanGUI-API niet.

### regel 113 — `display.disable()`  *(risico: mechanisch)*

**Bedoeling:** Toestand DISPLAY, tak HasFired(flagDrieSecKnop): apparaat uit, terug naar de wachttoestand. Let op: er staat geen puntkomma — het bestand is pseudocode, geen compileerbare C++. Geen CleanGUI-widget.

**Wordt:** Ongewijzigd laten. Raakt de CleanGUI-API niet.

