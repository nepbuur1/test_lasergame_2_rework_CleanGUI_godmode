# Migratiebrief: `apps/pong_cleangui_dg/pdg_StartPageDisplayControl.h`

6 call sites.

## Per call site

### regel 116 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Startpagina weer tekenbaar zetten; het commentaar L112-113 zegt expliciet dat dit dezelfde afspraak is als bij GameDisplayControl::showPage.

**Wordt:** Vervalt (show(true) doet de makeDrawable), of page.makeDrawable(true);.

### regel 117 — `page.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De hele startpagina (achtergrond + startknop) in een keer tekenen via de root.

**Wordt:** Ongewijzigd: page.show(true);.

### regel 118 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De startknop aanraakbaar maken; vandaag redundant naast show(true).

**Wordt:** Ongewijzigd laten. Dit is straks de enige regel die 'Click here to Start' laat werken — weghalen betekent een dode startknop.

### regel 123 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Tekenvlag uit voordat de hide() volgt, zodat het verbergen niets tekent (zie pdg_GameDisplayControl.h L329-341).

**Wordt:** page.makeNonDrawable(true);

### regel 124 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Vlaggen op 'uit' zonder te tekenen; bij de eerste aanroep (L143) heeft deze pagina zelfs nog nooit een pixel gezet.

**Wordt:** Schrappen — de nieuwe hide() repaint de eigen rect, en dat is precies wat hier niet mag (bij het opstarten viel zo'n wisvlak ooit over de startknop van een andere taak, zie crt_Panel.h L217-223).

### regel 125 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De startknop doof maken zolang de pagina uit beeld is; vandaag redundant naast hide(true).

**Wordt:** Ongewijzigd laten; wordt dragend. Zonder deze regel start een aanraking op de plek van de startknop straks een spel terwijl de spelpagina in beeld staat.

