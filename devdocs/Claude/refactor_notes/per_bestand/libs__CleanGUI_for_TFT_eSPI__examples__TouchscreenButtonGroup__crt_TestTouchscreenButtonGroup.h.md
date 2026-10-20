# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenButtonGroup/crt_TestTouchscreenButtonGroup.h`

1 call sites.

## Per call site

### regel 222 — `page1.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De opgebouwde pagina in een keer tekenen: achtergrondpaneel, knop A en B los, en de buttongroup met C en D erin. De knoppen moeten daarna reageren op aanraking.

**Wordt:** page1.show(true); ongewijzigd. Geen expliciete enable nodig: geen van de knoppen of de groep is ooit gedisabled en _bEnabled staat vanaf de constructor op true.

