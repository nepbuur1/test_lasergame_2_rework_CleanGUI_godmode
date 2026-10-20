# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/MultiPanel/crt_TestMultiPanel.h`

9 call sites.

## Per call site

### regel 265 — `button.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Alleen hertekenen: showButton() heeft net de bevelkleuren omgewisseld en wil de knop in-/uitgedrukt op het scherm zetten. Het commentaar erboven (regel 261-264) gaat uitsluitend over waarom de kinderen mee moeten (het label), niet over vlaggen.

**Wordt:** button.redraw(true);  // tekenen zonder vlaggen aan te raken

### regel 334 — `panel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Spawnen = het al opgebouwde spritepaneel weer laten meetekenen in de tree (zie het commentaar op regel 458-462: 'Spawning is no more than a position and setDrawable(true)').

**Wordt:** panel.makeDrawable(true);  — of samen met regel 335 tot één panel.show(true)

### regel 335 — `panel.show(false);		// puts it on the map straight away`  *(risico: mechanisch)*

**Bedoeling:** Het paneel nu meteen op de map blitten, zonder op het volgende beeld te wachten. Panel<0> heeft geen kinderen, dus bIncludeChildren=false is hier louter goedkoop.

**Wordt:** panel.redraw(false);  (regel 334+335 samen: panel.show(true);)

### regel 346 — `arSpritePanels[i].setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Sprite uit de tekening halen zonder per stuk te wissen; het wissen gebeurt bewust in één keer op regel 355 (commentaar 352-354: 'One repaint of the whole map rather than forty little erases').

**Wordt:** arSpritePanels[i].makeNonDrawable(true);  — expliciet NIET hide(), want de nieuwe hide() doet per sprite een repaintRegion en dat is precies de veertig-losse-wissen die dit commentaar vermijdt.

### regel 355 — `mapPanel.repaintRegion(mapPanel.getGlobPosPix(), mapPanel.getSizePix());`  *(risico: mechanisch)*

**Bedoeling:** De hele map in één keer terugzetten nadat alle sprites niet-tekenbaar zijn gemaakt.

**Wordt:** ongewijzigd — mits Widget::redrawRegion() zijn poort '!_bVisible' vervangt door '!_bDrawable'; blijft die poort op een verdwenen _bVisible staan, dan tekent dit niets meer.

### regel 424 — `fpsText.redraw();`  *(risico: mechanisch)*

**Bedoeling:** Alleen de fps-regel opnieuw schrijven; de tekst is opaak (regel 206), dus dat wist de vorige regel in dezelfde pass.

**Wordt:** ongewijzigd — maar let op de naambotsing: Text::redraw() (0 argumenten) verbergt straks Widget::redraw(bool). Deze aanroep blijft compileren; fpsText.redraw(true) zou dat niet.

### regel 471 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Alle 40 spritepanelen worden volledig opgebouwd en daarna uitgezet, zodat spawnen niets meer kost dan één blit (commentaar 458-462).

**Wordt:** panel.makeNonDrawable(true);

### regel 490 — `page1.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Het ene moment waarop het hele scherm wordt geschilderd (commentaar 488-489). Bedoeld is: teken alles wat tekenbaar is — niet: zet vlaggen.

**Wordt:** page1.redraw(true);

### regel 513 — `_pPage->show(true);		// alles opnieuw; goedkoper dan het slim doen`  *(risico: stil-gedrag)*

**Bedoeling:** Bij een gebufferde display elk beeld de hele tree opnieuw tekenen (zie de meting in het commentaar op 359-372: 10 -> 19 fps).

**Wordt:** _pPage->redraw(true);

## Leunt op het neveneffect van show/hide

- **regel 265**: Tweede aanroeper van dezelfde workaround: showButton() tekent hier ook een ingedrukte knop via show(true). Bovendien draait MultiPanel bij een gebufferde display elk beeld _pPage->show(true) (regel 513), wat vandaag de hele tree elk beeld opnieuw enablet terwijl er net een vinger op een knop ligt — alleen de guard in TouchscreenButton::enable voorkomt dat bPressed daarbij wordt weggegooid. Na de refactoring is er niets meer te voorkomen. De knoppen in dit voorbeeld worden nooit gedisabled, dus er leunt hier niets op de enable-kant zelf: er hoeft geen expliciete enable() bij.

## Observaties van de inventarisatie

In deze vier bestanden staat geen enkele hide() en geen enkele isShown(), dus de hide()->disable-kant komt hier niet voor; alleen de show()-kant is relevant. De grootste stille wijziging is hier niet enable maar drawable: show() wordt makeDrawable+redraw, en show() raakte _bDrawable vroeger nooit aan — elke page-brede show(true) maakt daarmee opzettelijk niet-tekenbare widgets weer tekenbaar (MultiPanel: de 40 spritepanelen uit regel 471, waarvan er dan veertig tegelijk op (0,0) verschijnen, elk beeld opnieuw via regel 513; ImageViewer: het kruisje; Inputfield: tsOutsideInputfieldButton, die in crt_InputField.h:70 met /*drawable*/false wordt gebouwd en dus onzichtbaar is voor wie alleen het voorbeeld leest). Daarom is redraw(true) de juiste vertaling van elke page-brede show(true), en show(true) juist niet. Widget::redrawRegion() en Widget::moveTo() beginnen allebei met 'if (!_bVisible) return;'; die poort moet _bDrawable worden, anders tekenen MultiPanel:355 en ImageViewer:480/490 niets meer en beweegt MultiPanel:409 / Inputfield:188 niets meer. Let daarbij op een subtiel verschil: vandaag cullt redrawRegion op _bVisible en slaat het tekenen pas in showPanel()/showText() over op _bDrawable, maar loopt wel door naar de kinderen — een _bDrawable-poort bovenaan cullt de kinderen mee. In deze vier voorbeelden maakt dat niets uit, omdat elke setDrawable() hier met bIncludeChildren=true gebeurt. Text::redraw() (nul argumenten) bestaat al en zal Widget::redraw(bool) verbergen: MultiPanel:424 en ImageViewer:496 blijven compileren, maar redraw(true) op een Text niet — overweeg Text::redraw() te schrappen of Widget::redraw een default-argument te geven. Panel::hide()/Text::hide() gebruiken isShown() om te bepalen of er ooit iets gestaan heeft (crt_Panel.h:226, crt_Text.h:196); die vraag verdwijnt netjes met de nieuwe hide()=makeNonDrawable+repaintRegion, want repaintRegion herstelt wat er hoort te staan in plaats van colBg te smeren. Buiten scope maar raakt het Inputfield-voorbeeld: TouchscreenKeyboard roept intern Base::show(true) aan op regels 263/295/327 bij een moduswissel, met dezelfde twee neveneffecten. Onzeker gelaten: of de nieuwe redraw() een default-argument krijgt, en of redrawRegion/moveTo hun poort op _bDrawable of op niets gaan zetten — de migratie van MultiPanel:355 en ImageViewer:480/490 hangt daarvan af.

