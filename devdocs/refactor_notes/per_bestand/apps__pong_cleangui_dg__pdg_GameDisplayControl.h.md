# Migratiebrief: `apps/pong_cleangui_dg/pdg_GameDisplayControl.h`

18 call sites.

## Per call site

### regel 324 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** De hele boom weer tekenbaar zetten voordat er getekend wordt — hij stond uit sinds hidePage().

**Wordt:** Vervalt: show(true) op de volgende regel doet de makeDrawable al. Wil je het expliciet houden: page.makeDrawable(true);

### regel 325 — `page.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De hele pagina in een keer tekenen via de PageRoot; het commentaar L315-320 legt uit dat show(true) op de root hier mag en zelfs moet, omdat redrawRegion() op een onzichtbare widget afbreekt.

**Wordt:** Ongewijzigd: page.show(true); (== makeDrawable + redraw). Alleen niet meer op het enable-neveneffect rekenen — dat staat al expliciet op L326.

### regel 326 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina aanraakbaar maken; vandaag redundant, want Widget::show(true) enablet de hele subtree al.

**Wordt:** Ongewijzigd laten. NIET opruimen als dubbelop: na de refactoring is dit de enige regel die de peddelknoppen nog aanraakbaar maakt.

### regel 344 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Eerst de tekenvlag uit, zodat de hide() erna zijn wisvlak overslaat — zie het commentaar L329-341: verbergen mag met opzet NIETS tekenen, omdat drie tekentaken dezelfde commandobuffer delen.

**Wordt:** page.makeNonDrawable(true); — en daarmee is het verbergen meteen compleet.

### regel 345 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De vlaggen op 'uit' zetten zonder te tekenen (het tekenen is al geblokkeerd door L344); de volgende pagina wist wat hier blijft staan met zijn eigen schermvullende backgroundPanel.

**Wordt:** Schrappen. De nieuwe hide() = makeNonDrawable + repaintRegion(eigen rect), en juist dat repainten is wat hier per se niet mag gebeuren: het zet tekenwerk in de gedeelde commandobuffer terwijl de volgende pagina daar al in schuift.

### regel 346 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina onaanraakbaar maken; vandaag redundant, want Widget::hide(true) disabelt de subtree al mee.

**Wordt:** Ongewijzigd laten. Wordt van redundant tot dragend: zonder deze regel blijven de peddelknoppen van een verborgen pagina op touch reageren.

### regel 410 — `paddlePanel.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De peddel in beeld brengen zodra PaddleControl hem opvraagt (flagShowPaddle).

**Wordt:** Ongewijzigd: paddlePanel.show(true);. Let op de volgorde binnen show(): de hide() op L419 zet de widget straks op non-drawable, dus de vlag moet gezet zijn voordat er getekend wordt.

### regel 411 — `paddlePanel.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Expliciet enablen naast de show — vandaag een no-op, want show() enablet al.

**Wordt:** Ongewijzigd laten; wordt de enige enable.

### regel 412 — `buttonLeft.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De linker peddelknop in beeld brengen: die hoort bij het lopende spel, niet bij de pagina als geheel.

**Wordt:** Ongewijzigd: buttonLeft.show(true);.

### regel 413 — `buttonLeft.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De knop weer op touch laten reageren (TouchscreenButton::touchPressed keert bij !isEnabled() meteen terug).

**Wordt:** Ongewijzigd laten; dit is straks de enige regel die de knop weer laat werken. Bijvangst: TouchscreenButton::enable() ziet nu een echte overgang uit->aan en gooit een verouderde bPressed weg — precies wat het commentaar op crt_TouchscreenButton.h L295-308 beschrijft.

### regel 414 — `buttonRight.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem voor de rechter peddelknop.

**Wordt:** Ongewijzigd: buttonRight.show(true);.

### regel 415 — `buttonRight.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem: de rechter knop weer laten reageren.

**Wordt:** Ongewijzigd laten; wordt de enige enable.

### regel 419 — `paddlePanel.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** De peddel uit beeld halen op flagHidePaddle (het spel is afgelopen), terwijl de spelpagina zelf nog wel in beeld staat — hier moet dus WEL gewist worden.

**Wordt:** Ongewijzigd: paddlePanel.hide(true);. Het wissen loopt straks via repaintRegion door de root in plaats van via een colBg-vulling; hier zelfde resultaat (alles eronder is zwart) en netjeser, want de doellijn komt terug in plaats van weggevuld te worden.

### regel 420 — `paddlePanel.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Expliciet disablen naast de hide — vandaag een no-op.

**Wordt:** Ongewijzigd laten; wordt de enige disable.

### regel 421 — `buttonLeft.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** De linker peddelknop weghalen: de knoppen horen bij het spel, niet bij de game-over-pagina (zie ook pdg_GameControl.h L61).

**Wordt:** Ongewijzigd: buttonLeft.hide(true);.

### regel 422 — `buttonLeft.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De knop echt doof maken, en via TouchscreenButton::disable() bPressed resetten zodat hij niet 'ingedrukt' blijft hangen.

**Wordt:** Ongewijzigd laten. Wordt dragend: het commentaar op crt_TouchscreenButton.h L278 ('Widget::hide() disabelt ook, dus dit dekt hide() mee') klopt straks niet meer, dus zonder deze regel blijft bPressed hangen en is de knop daarna dood.

### regel 423 — `buttonRight.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem voor de rechter peddelknop.

**Wordt:** Ongewijzigd: buttonRight.hide(true);.

### regel 424 — `buttonRight.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem: rechter knop doof maken en bPressed resetten.

**Wordt:** Ongewijzigd laten; wordt dragend, zie L422.

## Observaties van de inventarisatie

Geen enkele plek in deze vier bestanden leunt aantoonbaar op show()->enable of hide()->disable: elke show(true)/hide(true) heeft al een expliciete enable(true)/disable(true) ernaast (L325-326 en L344-346, L410-415, L419-424 in GameDisplayControl; L117-118/L124-125 in StartPage; L129-130/L136-137 in GameOver), vandaar dat reliesOnSideEffect leeg is. Het spiegelbeeld is hier wel het risico: die expliciete regels zijn vandaag redundant (Widget::show(true) enablet de hele subtree al, Widget::hide(true) disabelt hem) en worden na de refactoring de enige enable/disable — wie ze bij het opruimen als "dubbelop" schrapt, krijgt een dode startknop of een pagina die onzichtbaar blijft reageren. pdg_GameControl.h bevat geen enkele widget-aanroep; paddleControl.enable() (L53) en paddleControl.disable() (L62) zijn aanroepen op een control-object en lopen via showPaddle()/hidePaddle() naar de widgets in GameDisplayControl. Het gevoeligste punt is hidePage(): die functie bestaat er juist uit om NIETS te tekenen (commentaar pdg_GameDisplayControl.h L329-341 over drie tekentaken die een gedeelde commandobuffer vullen), dus de nieuwe hide() met zijn repaintRegion hoort daar helemaal niet meer te staan — alleen makeNonDrawable(true) + disable(true). Onzeker en te controleren in de bibliotheek: of de nieuwe redrawRegion() op _bDrawable gaat gaten nu _bVisible verdwijnt; doet hij dat, dan is "makeNonDrawable(true); hide(true);" ook nog veilig, maar het kost alsnog onnodige setClipRect/clearClipRect-commando's vanuit drie taken. Volgorde-val bij showPaddle (L410-414): omdat de nieuwe hide() de widget non-drawable maakt, moet show() de drawable-vlag zetten VOORDAT hij tekent — de huidige Panel::show() roept showPanel() eerst aan en zet de vlag daarna, en showPanel() keert bij !isDrawable() meteen terug, dus die volgorde moet in de bibliotheek omgedraaid worden of de peddel komt na het eerste spel niet meer terug. Twee dingen die niet in de gevraagde lijst staan maar wel op _bVisible leunen en dus meeveranderen: Widget::moveTo() keert vroeg terug op !_bVisible (gebruikt op L435 paddlePanel.moveTo en L441 ballPanel.moveTo) en Widget::redrawRegion() slaat een onzichtbare widget over — beide guards moeten _bDrawable worden, anders schuift een verborgen peddel weer zichtbaar mee. Commentaar dat achterhaald raakt en meegewijzigd moet worden: pdg_GameDisplayControl.h L296 en L311 ("show(true) tekent ze" — blijft kloppen), L315-320 ("redrawRegion() breekt af op een onzichtbare widget") en L329-330 ("setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat"), plus de verwijzingen daarnaar in pdg_StartPageDisplayControl.h L112-113 en pdg_GameOverDisplayControl.h L124-125. De wrappers showPage()/hidePage() worden aangeroepen op L381/L401/L464 (game), L143/L156/L168 (start) en L157/L170/L182 (gameover); die aanroepen zelf blijven ongewijzigd. In deze vier bestanden komen isShown(), redrawRegion(), repaintRegion() en paint() nergens voor; van de verdwijnende API staat alleen setDrawable( erin, zes keer, en dat zijn precies de zes compileerfouten.

