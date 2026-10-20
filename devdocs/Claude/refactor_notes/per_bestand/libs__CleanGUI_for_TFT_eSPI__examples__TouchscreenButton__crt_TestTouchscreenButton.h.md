# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenButton/crt_TestTouchscreenButton.h`

1 call sites.

## Per call site

### regel 154 — `page1.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina met achtergrondpaneel en de twee knoppen in een keer tekenen; daarna draait de lus alleen nog op de flags van de knoplisteners.

**Wordt:** page1.show(true); ongewijzigd. Ook hier geen enable nodig — niets is ooit gedisabled.

