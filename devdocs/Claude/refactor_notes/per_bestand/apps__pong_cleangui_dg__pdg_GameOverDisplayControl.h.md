# Migratiebrief: `apps/pong_cleangui_dg/pdg_GameOverDisplayControl.h`

6 call sites.

## Per call site

### regel 128 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Game-over-pagina weer tekenbaar zetten; commentaar L124-125 verwijst naar dezelfde afspraak als bij GameDisplayControl.

**Wordt:** Vervalt (show(true) doet de makeDrawable), of page.makeDrawable(true);.

### regel 129 — `page.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Achtergrond, GAME OVER-label met zijn tekstkind, en de startknop in een keer tekenen via de root.

**Wordt:** Ongewijzigd: page.show(true);.

### regel 130 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De startknop van de game-over-pagina aanraakbaar maken; vandaag redundant naast show(true).

**Wordt:** Ongewijzigd laten; straks de enige enable — anders is er geen weg meer uit de game-over-pagina.

### regel 135 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Tekenvlag uit voordat de hide() volgt, zodat het verbergen niets tekent.

**Wordt:** page.makeNonDrawable(true);

### regel 136 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Vlaggen op 'uit' zonder te tekenen; bij de eerste aanroep (L157) staat er nog niets van deze pagina op het scherm ('bij het eerste opstarten is er nog niets verloren').

**Wordt:** Schrappen — de nieuwe hide() zou hier wel tekenen, en de spelpagina die daarna in beeld komt zou daar zijn eigen tekenwerk mee kunnen kruisen.

### regel 137 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De startknop doof maken zolang de pagina uit beeld is; vandaag redundant naast hide(true).

**Wordt:** Ongewijzigd laten; wordt dragend.

