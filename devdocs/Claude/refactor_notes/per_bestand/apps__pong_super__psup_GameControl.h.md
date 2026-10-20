# Migratiebrief: `apps/pong_super/psup_GameControl.h`

7 call sites.

## Per call site

### regel 150 — `gameDisplayControl.showGamePage();`  *(risico: mechanisch)*

**Bedoeling:** Eerste regel van doGame(). Het commentaar op 143-146 legt uit wat dit is: 'Elke do..()-functie vraagt zijn eigen pagina op en geeft hem aan het eind weer vrij' - de garantie dat er nooit twee PageRoots tegelijk in beeld staan. Het is geen widgetaanroep maar een commando aan de displaytaak (zet flagShowGamePage, psup_GameDisplayControl.h:461-465).

**Wordt:** In DIT bestand ongewijzigd. Het widgetwerk zit in GameDisplayControl::showPage() (psup_GameDisplayControl.h:747-759); daar staat de expliciete page.enable(true) op 758 al naast page.show(true) op 757, dus de enable-kant gaat niet stil verloren. Wel moet daar page.show(true) -> page.redraw(true) worden: show()==makeDrawable+redraw zou het GAME OVER-vignet dat op 754 net non-drawable is gezet meteen weer tekenbaar maken, en dan begint elk spel met het vignet in beeld.

### regel 160 — `paddleControl.enable();`  *(risico: mechanisch)*

**Bedoeling:** 'De peddel eerst: die moet er staan voordat de bal eraan komt.' Zet de peddeltaak aan voordat de eerste bal wordt gespawnd (164).

**Wordt:** Ongewijzigd. Dit is PaddleControl::enable() (psup_PaddleControl.h:114-117), dat alleen flagEnable zet; het is geen Widget::enable(). De widgetkant volgt in psup_GameDisplayControl.h:1023-1034, waar setDrawable(true)+show(true)+enable(true) al drie losse, expliciete regels zijn per widget - daar verdwijnt door de refactoring niets stils.

### regel 217 — `paddleControl.disable();`  *(risico: mechanisch)*

**Bedoeling:** 'De peddelknoppen horen bij het spel, niet bij de game-over-pagina.' Peddel van het scherm en de twee peddelknoppen dood, vlak voordat het vignet komt.

**Wordt:** Ongewijzigd (PaddleControl, geen widget). Downstream in psup_GameDisplayControl.h:1036-1058 staan hide()/setDrawable(false) en disable(true) al als aparte regels. Let wel op de volgorde 217 -> 222: zie reliesOnSideEffect.

### regel 222 — `showGameOverLabel();`  *(risico: stil-gedrag)*

**Bedoeling:** UC04-uitzondering, letterlijk in het commentaar op 219-221: "Game Over" over het spelscherm heen, weg na een tik of drie seconden. 'Het spelbord blijft eronder staan; pas daarna gaat de pagina uit.' Aanroep van de eigen private functie op 295.

**Wordt:** De aanroep zelf blijft. Maar de aanname 'het spelbord blijft eronder staan' klopt onder de nieuwe API alleen nog als psup_GameDisplayControl.h:1066 pageGame.show(true) -> pageGame.redraw(true) wordt. show() = makeDrawable+redraw op de HELE pagina zet anders de peddel en de twee peddelknoppen die regel 217 net heeft weggehaald weer op tekenbaar, en tekent ze onder het vignet terug.

### regel 224 — `gameDisplayControl.hideGamePage();`  *(risico: mechanisch)*

**Bedoeling:** Laatste regel van doGame(): de pagina weer vrijgeven, zie het commentaar op 143-146.

**Wordt:** In dit bestand ongewijzigd. In GameDisplayControl::hidePage() (774-786) staat de expliciete page.disable(true) al; de eis uit het commentaar 761-773 (verbergen mag NIETS tekenen, anders schuiven twee tekentaken door elkaar) blijft ook onder de nieuwe API haalbaar: hide() = makeNonDrawable + repaintRegion, en na makeNonDrawable op de hele boom levert die repaint niets meer op. De regel page.setDrawable(false, true) op 776 wordt daar dan overbodig (en verdwijnt sowieso als naam).

### regel 300 — `gameDisplayControl.showGameOverLabel();`  *(risico: stil-gedrag)*

**Bedoeling:** Het vignet tonen EN aantikbaar maken. Dat tweede staat nergens met zoveel woorden, maar volgt uit regel 306: er wordt op flagGameOverLabelPressed gewacht, en die kan alleen komen als buttonGameOverLabel enabled is (TouchscreenButton::touchPressed keert meteen terug bij !isEnabled(), crt_TouchscreenButton.h:219).

**Wordt:** Aanroep ongewijzigd. Voorwaarde: de expliciete buttonGameOverLabel.enable(true) op psup_GameDisplayControl.h:1065 MOET blijven staan. Vandaag doen twee dingen het aanzetten (die enable, en de enable-lawine van pageGame.show(true) op 1066); na de refactoring is die enable de enige. Zie reliesOnSideEffect.

### regel 319 — `gameDisplayControl.hideGameOverLabel();`  *(risico: mechanisch)*

**Bedoeling:** Vignet weg zodra er getikt is of de drie seconden om zijn, nog voordat doGame() de pagina vrijgeeft (224).

**Wordt:** Ongewijzigd. In psup_GameDisplayControl.h:1070-1071 staat setDrawable(false,true) + disable(true); dat wordt makeNonDrawable(true) + disable(true). Niet verleid raken om daar hide(true) van te maken: hide() gaat onder de nieuwe API wel repainten, en het vignet hoort juist zonder tekenwerk te verdwijnen (zelfde reden als 761-773).

## Leunt op het neveneffect van show/hide

- **regel 222**: De volgorde 217 -> 222 leunt op de huidige betekenis van show(). Na paddleControl.disable() staan paddlePanel, buttonLeft en buttonRight op non-drawable (buffered pad, psup_GameDisplayControl.h:1045-1047) of zijn ze gewist (niet-buffered pad, 1051-1053). Direct daarna laat showGameOverLabel() de displaytaak pageGame.show(true) doen (1066). Vandaag raakt dat _bDrawable niet aan, dus in buffered modus blijven ze weg. Onder de nieuwe API is show(true) == makeDrawable(true) + redraw(true) over de hele boom en komen peddel en peddelknoppen terug in beeld onder het GAME OVER-vignet. Fix hoort in de displayklasse (redraw(true)), maar het is deze volgorde die hem uitlokt.
- **regel 280**: spawnables.publishStates(score); in updateSpawnables() - en hetzelfde op 214 na afloop van de spellus. Dit gaat via Spawnables::publishStates -> IGameDisplayControl::updateGameObjects -> flagBallPositionUpdated naar het per-beeld pageGame.show(true) op psup_GameDisplayControl.h:1145. Dat is de zwaarste show()-op-een-pagina in dit spel: applyBallStates/applyCrateStates/applyFireBinStates hebben net alle ongebruikte slots op setDrawable(false) gezet, en onder de nieuwe API zou makeDrawable(true, true) die vijftig balpanels, vijf crates en vijf vuurkorven elk beeld weer aanzetten - alle vijftig ballen tegelijk in beeld, 25 keer per seconde. Moet daar redraw(true) worden. In dit bestand hoeft niets te veranderen, maar deze regel is wel de aanjager van die aanroep.
- **regel 299**: flagGameOverLabelPressed.clear(); vlak voor showGameOverLabel() - en het tweede exemplaar op 313. Dit is een bestaande workaround die NIET mag verdwijnen. Reden: het vignet is ook tijdens het spel enabled. showPage() disabelt hem op psup_GameDisplayControl.h:755, maar de page.enable(true) op 758 zet hem een regel later weer aan, dus een tik midden in het speelveld tijdens het spel zet de vlag al. De clear op 299 vangt dat op. enable/disable veranderen niet door deze refactoring, dus die situatie blijft precies zoals hij is. (De clear op 313 heeft een andere reden, in het commentaar 308-312 uitgelegd: waitAny laat de event-bits staan.)
- **regel 306**: pTask->waitAny(flagGameOverLabelPressed + timerGameOverLabel); - dit is het duidelijkste 'wat er NIET staat'. Deze klasse enablet nergens zelf een widget, ook niet pGameOverLabel (dat kent hij wel, hij bewaart de pointer op 111 en vergelijkt hem op 132). Dat de tik ooit binnenkomt hangt volledig aan wat showGameOverLabel() op 300 in de displaytaak losmaakt. Vandaag zetten daar twee dingen de knop aan: de expliciete buttonGameOverLabel.enable(true) (psup_GameDisplayControl.h:1065) en het neveneffect van pageGame.show(true) op 1066. Na de refactoring blijft alleen de eerste over. Schrapt iemand die tijdens het opruimen ('show zet hem toch aan'), dan reageert het vignet niet meer op aanraking en duurt UC04 altijd de volle GameOverLabelTimeoutMs - geen compilerfout, geen crash, alleen een spel dat trager aanvoelt.

## Observaties van de inventarisatie

Letterlijke treffers op show(/hide(/enable(/disable(/setDrawable(/isShown(/redrawRegion(/repaintRegion(/paint( in dit bestand: precies TWEE - regel 160 en regel 217. Allebei PaddleControl::enable()/disable() (psup_PaddleControl.h:114-122), dus flagzetters op een Task, geen Widget-methodes. Er staat in dit bestand geen enkele directe widgetaanroep: geen setDrawable, geen isShown, geen redrawRegion/repaintRegion/paint. Dat betekent NUL compileerfouten hier.

Alle andere plekken die ik meld (150, 222, 224, 300, 319) zijn commando's aan IGameDisplayControl - showGamePage/hideGamePage/showGameOverLabel/hideGameOverLabel, gedefinieerd op psup_GameDisplayControl.h:461-501 als vlagzetters. Ze matchen niet op 'show(' maar ze zijn wel de plek waar dit bestand widgetgedrag bestelt, en ze zijn de reden dat de vier plekken in de displayklasse load-bearing zijn.

Bestaande workarounds: er zit in dit bestand GEEN enkele enable()/disable() die vlak na een show()/hide() staat om de enable-toestand recht te zetten, en geen commentaar in de trant van 'show zet de enable-toestand terug'. Er valt hier dus niets op te ruimen. De enige regel die op een workaround lijkt (flagGameOverLabelPressed.clear() op 299) heeft een andere oorzaak - het vignet is tijdens het spel enabled door page.enable(true) - en moet blijven staan.

Wat dit bestand nodig heeft om na de refactoring hetzelfde te doen, zit dus volledig in psup_GameDisplayControl.h. Vier concrete punten daar, allemaal uitgelokt vanuit doGame()/updateSpawnables():
  1. regel 757  page.show(true) in showPage()      -> redraw(true)  (anders staat het vignet meteen in beeld bij elk nieuw spel: 754 zet hem net op non-drawable)
  2. regel 1066 pageGame.show(true) bij vignet     -> redraw(true)  (anders komen peddel + peddelknoppen terug, zie 217/222)
  3. regel 1145 pageGame.show(true) per beeld      -> redraw(true)  (anders alle 50 ballen tegelijk in beeld)
  4. regel 1065 buttonGameOverLabel.enable(true)   -> laten staan   (enige enable die overblijft; regel 306 hier hangt eraan)
Verder verdwijnen daar de setDrawable-aanroepen (compileerfouten) op 571, 585, 598, 684, 750, 754, 776, 846, 850, 869, 873, 901, 905, 1025-1027, 1045-1047, 1064, 1070, en Panel::hide() gebruikt WBase::isShown() (crt_Panel.h:226).

Aanname die ik NIET heb geverifieerd: ik ben ervan uitgegaan dat redrawRegion() onder de nieuwe API op !_bDrawable gaat afbreken waar hij nu op !_bVisible afbreekt (crt_Widget.h:357), en dat hide() zijn makeNonDrawable vóór de repaintRegion doet. Alleen dan blijft hidePage() (psup_GameDisplayControl.h:774-786) net zo tekenloos als het commentaar op 761-773 eist, en blijft regel 224 hier veilig.

