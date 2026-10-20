# Migratiebrief: `apps/pong_simple/psim_DisplayControl.h`

19 call sites.

## Per call site

### regel 446 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** In showPage(): eerst de hele pagina-subboom weer tekenbaar maken, want hidePage() had hem niet-tekenbaar gezet en Panel::showPanel() keert bij !isDrawable() meteen terug. Moet dus per se vóór de show() op 447 staan.

**Wordt:** Vervalt: `page.show(true)` op 447 doet de makeDrawable(true) nu zelf (show == makeDrawable + redraw, in die volgorde). Wie hem letterlijk wil houden: `page.makeDrawable(true);`.

### regel 447 — `page.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De pagina nu echt tekenen, inclusief alle kinderen (panelen, knoppen, tekst). Het commentaar op 442-443 zegt waarom dat mag zonder eerst te wissen: elke pagina begint met een schermvullend paneel, dus hij overschrijft wat de vorige achterliet.

**Wordt:** Blijft `page.show(true);` — tekent nog steeds. De enable die er vandaag gratis bij zit verdwijnt stil, maar wordt hier al gedekt door de expliciete `page.enable(true)` op 448.

### regel 448 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De knoppen van deze pagina weer op aanraking laten reageren. Vandaag dubbelop: Widget::show() roept per widget al enable(false) aan, dus na show(true) is de hele subboom al enabled.

**Wordt:** Ongewijzigd, maar wordt van redundant naar dragend: dit is straks de enige enable. Ook TouchscreenButton::enable()'s opruimen van een blijven hangen bPressed (zie crt_TouchscreenButton.h:295-310) hangt hier vanaf. Niet opruimen.

### regel 453 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** In hidePage(): met opzet vóór de hide(), zodat Panel::hide() zijn wisvlak overslaat (crt_Panel.h:322-330 keert terug bij !isDrawable()). Commentaar 435-443: het verbergen tekent expres niets — dat scheelt de zwarte flits tussen twee pagina's, en het mag omdat de volgende pagina schermvullend begint.

**Wordt:** `page.makeNonDrawable(true);` — een-op-een, en dit is nu precies de hele bedoeling van hidePage()'s eerste twee regels.

### regel 454 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Alleen nog de vlag _bVisible uitzetten; door de setDrawable(false) ervan tekent en wist deze aanroep vandaag helemaal niets.

**Wordt:** Vervalt. In de nieuwe API is regel 453 (makeNonDrawable) exact wat hier bedoeld werd; hide() zou er een repaintRegion over de hele paginarect bij doen. Die repaint tekent hier weliswaar niets (alles is dan al niet-tekenbaar), maar hij is overbodig en hij wordt fout zodra iemand de volgorde omdraait — hide() vóór makeNonDrawable wist wél. Let op dat de disable die hide() vandaag impliceert verdwijnt; regel 455 dekt dat al.

### regel 455 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De knoppen van deze pagina niet meer op aanraking laten reageren. Vandaag dubbelop: Widget::hide() disablet elke widget al.

**Wordt:** Ongewijzigd, maar wordt dragend: dit is straks de enige disable, en ook het enige dat nog TouchscreenButton::disable() bereikt (die reset bPressed — zonder dat blijft een knop die onder je vinger verdwijnt voorgoed 'ingedrukt', zie crt_TouchscreenButton.h:270-283). Niet opruimen.

### regel 482 — `pageRoot.show(false);`  *(risico: mechanisch)*

**Bedoeling:** Alleen de vlag omzetten, expres zonder kinderen. Het commentaar 473-481 legt uit waarom: redrawRegion() breekt af op een onzichtbare widget, dus zonder deze regel verschijnt het beeld wel maar herstelt moveTo() daarna niets meer; show(true) zou alle drie de pagina's tegelijk tonen; de root zelf is een kale Widget en tekent niets.

**Wordt:** `pageRoot.makeDrawable(false);`. Strikt genomen mag de regel ook weg: _bDrawable staat bij constructie al op true (crt_Widget.h:50), dus de poort waar dit commentaar over gaat is er straks vanzelf. Het commentaarblok 473-481 moet hoe dan ook herschreven — de reden die het geeft (_bVisible) bestaat niet meer.

### regel 567 — `paddlePanel.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bij flagShowPaddle: de peddel weer in beeld zetten aan het begin van een spel (PaddleControl vraagt dit via displayControl.showPaddle(), psim_PaddleControl.h:174).

**Wordt:** Blijft `paddlePanel.show(true);`. Werkt ook na de refactoring goed: de hide op 576 maakt hem straks niet-tekenbaar en deze show maakt hem weer tekenbaar. De verdwenen enable is gedekt door 568.

### regel 568 — `paddlePanel.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Administratief: de peddel is een Panel, geen knop, en niets in de codebase leest isEnabled() van een Panel. Staat er voor de symmetrie met de disable op 577.

**Wordt:** Ongewijzigd. Mag blijven staan; heeft na de refactoring net zo min functioneel effect als nu.

### regel 569 — `buttonLeft.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De linker peddelknop weer tekenen. Bedoeld is nadrukkelijk óók dat hij weer aanraakbaar wordt — de knop is op 579 gedisabled toen de peddel verdween.

**Wordt:** Blijft `buttonLeft.show(true);`. De enable die vandaag in show() zit verdwijnt stil, maar 570 staat er al. Zonder 570 zou de knop na de refactoring zichtbaar maar dood zijn.

### regel 570 — `buttonLeft.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Vandaag redundant (show(true) enablede hem al één regel eerder).

**Wordt:** Ongewijzigd, maar wordt dragend: dit is straks de enige enable van de linker peddelknop, én de plek waar TouchscreenButton::enable() een verouderde bPressed weggooit.

### regel 571 — `buttonRight.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Idem als 569, voor de rechter peddelknop.

**Wordt:** Blijft `buttonRight.show(true);`; de verdwenen enable is gedekt door 572.

### regel 572 — `buttonRight.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Vandaag redundant.

**Wordt:** Ongewijzigd, wordt dragend. Niet opruimen.

### regel 576 — `paddlePanel.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bij flagHidePaddle: de peddel van het scherm halen als het spel afloopt. Hier wordt géén setDrawable(false) vooraf gedaan (anders dan in hidePage), dus dit wist wél: Panel::hidePanel() vult de peddelrect met _colBg (zwart).

**Wordt:** Blijft `paddlePanel.hide(true);`. Gedragsverschil: nieuw wist hij niet met zijn eigen _colBg maar laat hij de boom de rect opnieuw tekenen — hier levert dat hetzelfde zwart op (gamePanel colFg 0x00000000). De verdwenen disable is gedekt door 577.

### regel 577 — `paddlePanel.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Administratief, zie 568: een Panel z'n enable-toestand wordt nergens gelezen.

**Wordt:** Ongewijzigd.

### regel 578 — `buttonLeft.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De linker peddelknop weghalen als het spel afloopt — zichtbaar én aanraakbaar. Dat laatste is niet cosmetisch: op de game-over-pagina mag een druk op de oude peddelknop niets meer doen.

**Wordt:** Blijft `buttonLeft.hide(true);`. De disable verdwijnt stil uit hide(); 579 staat er al. Zonder 579 zou de knop onzichtbaar maar aanraakbaar blijven.

### regel 579 — `buttonLeft.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Vandaag redundant (hide(true) disablede al).

**Wordt:** Ongewijzigd, wordt dragend — en dit is straks de enige route naar TouchscreenButton::disable(), die bPressed reset. Niet opruimen.

### regel 580 — `buttonRight.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Idem als 578, voor de rechter peddelknop.

**Wordt:** Blijft `buttonRight.hide(true);`; de verdwenen disable is gedekt door 581.

### regel 581 — `buttonRight.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Vandaag redundant.

**Wordt:** Ongewijzigd, wordt dragend. Niet opruimen.

## Observaties van de inventarisatie

Geen enkele plek in deze twee bestanden leunt op het neveneffect: achter élke show/hide op een widget staat al een expliciete enable/disable (447+448, 454+455, 567+568, 569+570, 571+572, 576+577, 578+579, 580+581), en de drie pagina's krijgen bij het opbouwen hun disable via hidePage() op 495, 503 en 509 — dus reliesOnSideEffect is leeg. Het belangrijkste gevolg: juist die 'dubbele' calls, die vandaag redundant zijn, worden na de refactoring dragend en mogen niet als opschoning verdwijnen (448, 455, 570, 572, 579, 581 zijn functioneel; 568 en 577 zijn puur administratief, want alleen TouchscreenButton leest isEnabled()). Wat wél weg kan: regel 446 (show doet de makeDrawable nu zelf) en regel 454 (makeNonDrawable op 453 ís al wat hier bedoeld werd); regel 482 mag ook weg omdat _bDrawable bij constructie al true is (crt_Widget.h:50). De workaround die de nieuwe API overbodig maakt is de constructie hidePage()/commentaarblok 435-443: "setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat" is precies wat makeNonDrawable per definitie doet. Volgorde-waarschuwing: in hidePage moet het niet-tekenbaar maken vóór elke mogelijke repaint blijven staan — hide() vóór makeNonDrawable wist wél, en dat is exact de zwarte flits tussen twee pagina's die de auteur hier vermijdt. Commentaar dat feitelijk onwaar wordt en mee moet in de refactoring: regel 434 (somt setDrawable op), 437-438, 473-481 (redrawRegion gate op _bVisible) en 594-595 ("onzichtbare widget" → "niet-tekenbare widget"); regel 409 en 428 ("show(true) tekent ze") blijven kloppen. Semantische verschuiving buiten deze bestanden om, maar hier wel voelbaar: de startwaarde was _bVisible=false (er tekent niets tot de eerste show), na de refactoring is _bDrawable=true de startwaarde, dus alles moet expliciet niet-tekenbaar gemaakt worden bij het opbouwen — hier gebeurt dat al op 495/503/509, maar in het venster tussen addChildWidget en die hidePage-aanroepen is een subboom straks wél tekenbaar. Let bij het implementeren van hide(bIncludeChildren) op de parent/kind-volgorde: als de ouder zijn rect repaint terwijl een kind nog drawable is, blijft dat kind staan — regel 578 en 580 zijn precies dat geval (een TouchscreenButton met een Text-kind). Ik weet niet zeker hoe redrawRegion in de nieuwe API met een niet-tekenbare ouder omgaat (subboom overslaan of toch doorlopen); dat bepaalt of het weglaten van `page.hide(true)` op 454 echt niets verandert — mijn analyse gaat ervan uit dat een niet-tekenbare widget niets tekent, wat in beide varianten waar is. psim_GameControl.h bevat geen enkele widget-aanroep: alleen displayControl.showGamePage()/hideGamePage() (regel 43 en 59, dat zijn vlagzetters die uiteindelijk op showPage/hidePage uitkomen) en de twee PaddleControl-calls hierboven.

