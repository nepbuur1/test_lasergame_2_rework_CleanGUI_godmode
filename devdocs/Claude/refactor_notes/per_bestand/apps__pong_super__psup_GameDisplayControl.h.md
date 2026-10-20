# Migratiebrief: `apps/pong_super/psup_GameDisplayControl.h`

42 call sites.

## Per call site

### regel 571 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** initPanels(), balpanels. Commentaar op regel 555-558: alle MaxNofBalls panels worden volledig opgezet en daarna uitgezet; 'een bal in het spel zetten is dan niet meer dan een positie en setDrawable(true)'. Puur een vlag, er staat op dit moment nog niets op het scherm.

**Wordt:** panel.makeNonDrawable(true);

### regel 585 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem voor de cratepanels: opgezet en meteen uit, tot applyCrateStates() ze aanzet.

**Wordt:** panel.makeNonDrawable(true);

### regel 598 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem voor de vuurkorfpanels.

**Wordt:** panel.makeNonDrawable(true);

### regel 684 — `buttonGameOverLabel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** initPanels(): het GAME OVER-vignet staat er wel, maar mag bij het opbouwen niets tekenen. Er staat GEEN disable() naast, terwijl deze knop op regel 407 al als touchlistener is aangemeld en _bEnabled per constructor true is - hij is dus aanraakbaar-maar-onzichtbaar tot hidePage() op regel 990 hem uitzet.

**Wordt:** buttonGameOverLabel.makeNonDrawable(true); en overweeg er meteen buttonGameOverLabel.disable(true); bij te zetten, zodat het niet meer van hidePage() afhangt.

### regel 750 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** showPage(): de hele pagina weer tekenbaar maken nadat hidePage() alles op niet-tekenbaar had gezet. Let op de volgorde: dit staat NA initBallAndPaddlePosition() (regel 749), dat via applyBallStates/applyCrateStates/applyFireBinStates juist de ongebruikte slots uitzet - die worden hier dus allemaal weer aangezet.

**Wordt:** page.makeDrawable(true); en verplaats hem VOOR initBallAndPaddlePosition(), anders staan straks alle 50 balpanels, 5 crates en 5 vuurkorven weer aan.

### regel 754 — `buttonGameOverLabel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Commentaar erboven: 'setDrawable(true, true) hierboven zet ook het vignet aan; een nieuw spel begint zonder.' Corrigeert regel 750 voor deze ene widget.

**Wordt:** buttonGameOverLabel.makeNonDrawable(true); - en dit MOET blijven staan tussen het makeDrawable van de pagina en het tekenen, want als regel 757 show(true) blijft, zet die het vignet weer tekenbaar (nieuwe show == makeDrawable + redraw).

### regel 755 — `buttonGameOverLabel.disable(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeling: tijdens het spel is het vignet niet aantikbaar. Vandaag doet deze regel NIETS: regel 757 page.show(true) enablet via Widget::show de hele boom weer, en regel 758 page.enable(true) doet dat nog eens. Het vignet is tijdens het hele spel onzichtbaar maar touch-live.

**Wordt:** Ongewijzigde aanroep, maar VERPLAATSEN naar na page.enable(true) (regel 758). Pas dan doet hij wat er staat.

### regel 757 — `page.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De hele spelpagina in beeld zetten. Commentaar op regel 741-746: show(true) op de PageRoot zelf mag en moet hier, want redrawRegion() breekt af op een onzichtbare widget en moveTo() laat de bal via de root hertekenen. Het enable-neveneffect is hier niet bedoeld (regel 758 doet het expliciet).

**Wordt:** page.redraw(true); - NIET show(true) laten staan: de nieuwe show() zet eerst _bDrawable op de hele boom en maakt daarmee regel 754 (vignet) en alle uitgezette bal-/crate-/vuurkorfslots weer tekenbaar.

### regel 758 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina reageert weer op aanraking (peddelknoppen). Vandaag dubbelop, want show(true) op regel 757 enablet de hele boom al.

**Wordt:** Ongewijzigd - maar hij wordt van overbodig naar dragend: dit is straks de ENIGE plek die de peddelknoppen van deze pagina weer inschakelt bij het starten van een spel.

### regel 776 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** hidePage(). Commentaar op regel 761-773: uit beeld en met OPZET zonder iets te tekenen - setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat. Dat is wat de opzet met een tekentaak per pagina werkbaar houdt: zou verbergen wel tekenen, dan schuift deze taak een zwart vlak in de commandobuffer terwijl de volgende pagina daar al in tekent.

**Wordt:** page.makeNonDrawable(true); - dit is straks de hele hidePage-tekenkant, regel 777 vervalt.

### regel 777 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeld als 'markeer de pagina als niet-getoond' (en, vandaag meegeleverd, disable de hele boom). Tekenen doet hij hier bewust niet: Panel::hide() -> hidePanel() keert meteen terug omdat regel 776 alles net niet-tekenbaar heeft gemaakt.

**Wordt:** VERWIJDEREN. De nieuwe hide() == makeNonDrawable + repaintRegion(eigen rect) en zou dus juist wel tekencommando's de buffer in schuiven - precies wat het commentaar op regel 761-773 verbiedt. makeNonDrawable op regel 776 dekt de vlag, disable(true) op regel 778 dekt het enable-neveneffect.

### regel 778 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina mag geen aanrakingen meer aannemen als hij uit beeld is. Vandaag dubbelop met het disable-neveneffect van hide() op regel 777.

**Wordt:** Ongewijzigd - wordt dragend zodra regel 777 weg is. Dit is de enige plek die buttonLeft, buttonRight en buttonGameOverLabel uitzet bij het verlaten van de spelpagina.

### regel 846 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** applyFireBinStates(): een slot dat niet in het spel is wordt niet-tekenbaar, en zo verdwijnt een verlopen vuurkorf vanzelf. Alleen een vlag; het tekenen gebeurt daarna in een keer (regel 891).

**Wordt:** panel.makeNonDrawable(true);

### regel 850 — `panel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** applyFireBinStates(): actief slot weer meelaten tekenen. Alleen een vlag, geen tekenwerk.

**Wordt:** panel.makeDrawable(true);

### regel 869 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** applyCrateStates(): commentaar regel 857-859 - 'een slot dat niet in het spel is wordt niet-tekenbaar, en zo verdwijnt een geopende crate vanzelf'.

**Wordt:** panel.makeNonDrawable(true);

### regel 873 — `panel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** applyCrateStates(): actieve crate weer meetekenen.

**Wordt:** panel.makeDrawable(true);

### regel 901 — `panel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** applyBallStates(): commentaar regel 886-889 - zo verdwijnt een bal die zichzelf aan het depot teruggaf van het scherm, zonder apart bericht. Bewust alleen de vlag: 'Alleen de coordinaten; het tekenen gebeurt daarna in een keer.'

**Wordt:** panel.makeNonDrawable(true);

### regel 905 — `panel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** applyBallStates(): balslot dat in het spel is weer meetekenen.

**Wordt:** panel.makeDrawable(true);

### regel 1025 — `paddlePanel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** STATE_SHOWN, flagShowPaddle: peddel en peddelknoppen weer tekenbaar maken voordat ze getekend worden. Eerst alledrie de vlaggen, dan pas tekenen.

**Wordt:** Vervalt: paddlePanel.show(true) op regel 1029 doet dit nu zelf (show == makeDrawable + redraw). Wil je de twee stappen gescheiden houden, dan makeDrawable(true).

### regel 1026 — `buttonLeft.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem voor de linkerpeddelknop.

**Wordt:** Vervalt (buttonLeft.show(true) op 1031 doet het), anders makeDrawable(true).

### regel 1027 — `buttonRight.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem voor de rechterpeddelknop.

**Wordt:** Vervalt (buttonRight.show(true) op 1033 doet het), anders makeDrawable(true).

### regel 1029 — `paddlePanel.show(true);`  *(risico: mechanisch)*

**Bedoeling:** De peddel nu tekenen (hij was net tekenbaar gemaakt). Het enable-neveneffect is hier niet bedoeld; regel 1030 doet dat expliciet.

**Wordt:** Ongewijzigd: show(true) == makeDrawable + redraw, precies wat 1025+1029 samen deden. paddlePanel heeft geen kinderen en overlapt niets, dus de volgordewissel is onschadelijk.

### regel 1030 — `paddlePanel.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Symmetrisch met de disable op regel 1056. Een Panel doet met _bEnabled niets (alleen TouchscreenButton kijkt ernaar), dus dit is decoratief; vandaag deed show(true) het toch al.

**Wordt:** Ongewijzigd (of weg te laten - een Panel gebruikt _bEnabled niet).

### regel 1031 — `buttonLeft.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Linkerpeddelknop tekenen.

**Wordt:** Ongewijzigd (show == makeDrawable + redraw, vervangt 1026+1031).

### regel 1032 — `buttonLeft.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De knop moet weer aanrakingen aannemen. Vandaag dubbelop met het enable-neveneffect van show() op 1031; die enable liep bovendien via TouchscreenButton::enable, die bij een echte uit->aan-overgang bPressed opruimt.

**Wordt:** Ongewijzigd - wordt dragend. Zorg dat hij NA de show blijft staan; TouchscreenButton::enable() wist bPressed alleen bij een echte overgang, en die overgang komt straks alleen nog hiervandaan.

### regel 1033 — `buttonRight.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Rechterpeddelknop tekenen.

**Wordt:** Ongewijzigd (vervangt 1027+1033).

### regel 1034 — `buttonRight.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Rechterknop weer aanraakbaar; vandaag dubbelop met show().

**Wordt:** Ongewijzigd - wordt dragend.

### regel 1045 — `paddlePanel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** flagHidePaddle, tak MET backbuffer. Commentaar regel 1040-1044: 'setDrawable(false) en niet hide(): het eerstvolgende beeld bouwt de hele boom opnieuw op en zou hem dan gewoon terugzetten. Wissen hoeft er niet bij.'

**Wordt:** paddlePanel.makeNonDrawable(true); - en de reden uit het commentaar wordt sterker: met de nieuwe hide() zou er ook nog een repaintRegion overheen gaan.

### regel 1046 — `buttonLeft.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem, linkerknop uit beeld in de gebufferde tak.

**Wordt:** buttonLeft.makeNonDrawable(true);

### regel 1047 — `buttonRight.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem, rechterknop.

**Wordt:** buttonRight.makeNonDrawable(true);

### regel 1051 — `paddlePanel.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** flagHidePaddle, tak ZONDER backbuffer: de peddel moet nu echt van het scherm gewist worden, want er komt geen volledige hertekening meer overheen. Vandaag vult Panel::hidePanel() dat vlak met _colBg; _bDrawable blijft daarbij true.

**Wordt:** Ongewijzigd houden: de nieuwe hide() == makeNonDrawable + repaintRegion(eigen rect) herstelt wat eronder hoort (het achtergrondpanel) in plaats van er _colBg overheen te kladden - beter. Dat _bDrawable nu ook uitgaat repareert bovendien de fout hieronder (zie regel 1066).

### regel 1052 — `buttonLeft.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem, linkerpeddelknop wissen in de ongebufferde tak.

**Wordt:** Ongewijzigd; de disable die dit vandaag meebracht staat expliciet op regel 1057.

### regel 1053 — `buttonRight.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem, rechterpeddelknop.

**Wordt:** Ongewijzigd; disable staat expliciet op regel 1058.

### regel 1056 — `paddlePanel.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Buiten de if/else, dus voor beide takken: de peddel is weg en mag niet meer meedoen. Voor een Panel decoratief, maar het houdt de drie regels symmetrisch.

**Wordt:** Ongewijzigd.

### regel 1057 — `buttonLeft.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Expliciet buiten de if/else gezet, juist omdat de gebufferde tak alleen setDrawable(false) doet en dus niets uitschakelt. Dit is precies de bestaande 'workaround' voor het ontbrekende disable-neveneffect in die tak.

**Wordt:** Ongewijzigd - blijft nodig, en is straks in beide takken de enige disable.

### regel 1058 — `buttonRight.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Idem voor de rechterknop.

**Wordt:** Ongewijzigd - blijft nodig.

### regel 1064 — `buttonGameOverLabel.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** flagShowGameOverLabel: het vignet mag weer tekenen. Commentaar regel 1062-1063: 'Er komen geen balbeelden meer, dus deze pagina wordt nergens anders meer hertekend. Zelf doen dus.'

**Wordt:** buttonGameOverLabel.makeDrawable(true);

### regel 1065 — `buttonGameOverLabel.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Het vignet moet aantikbaar worden (UC04-uitzondering: weg na een tik of na 3 s). Vandaag deed pageGame.show(true) op 1066 dat ook al.

**Wordt:** Ongewijzigd - wordt dragend.

### regel 1066 — `pageGame.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De hele pagina een keer hertekenen zodat het vignet erop komt te staan - er komen geen balbeelden meer die dat zouden doen.

**Wordt:** pageGame.redraw(true); - MOET veranderen. De nieuwe show(true) zet _bDrawable op de hele boom en haalt daarmee de zojuist verborgen peddel + peddelknoppen (regels 1045-1047 / 1051-1053) en alle uitgezette bal-, crate- en vuurkorfslots terug op het scherm.

### regel 1070 — `buttonGameOverLabel.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** flagHideGameOverLabel: het vignet gaat uit. Bewust zonder wissen - direct hierna volgt hideGamePage() en tekent de volgende pagina zijn eigen achtergrond eroverheen (zie de redenering bij hidePage, regel 761-773).

**Wordt:** buttonGameOverLabel.makeNonDrawable(true); - juist NIET hide(), want die repaint straks de vignetregio en dat is precies het tekenen tussen twee paginataken in dat hier vermeden wordt.

### regel 1071 — `buttonGameOverLabel.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Het vignet is weg en mag geen tik meer aannemen. Vandaag vrijwel zinloos: het eerstvolgende pageGame.show(true) (1066/1145) zou hem weer enablen - alleen komt dat er in deze volgorde niet meer, want hierna volgt hidePage().

**Wordt:** Ongewijzigd - wordt dragend.

### regel 1145 — `pageGame.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De beeldlus, gebufferde tak, 25x per seconde: 'Dit is het beeld.' De hele boom opnieuw opbouwen in de backbuffer; AsyncDisplay stuurt hem daarna zelf uit. Bedoeld is puur tekenen.

**Wordt:** pageGame.redraw(true); - MOET veranderen, dit is de gevaarlijkste regel van het bestand. De nieuwe show(true) zou elk beeld _bDrawable op de hele boom zetten en daarmee alle 50 balpanels, 5 crates, 5 vuurkorven, de verborgen peddel/peddelknoppen (1045-1047) en het vignet weer aanzetten - het beeld loopt dan meteen vol.

## Leunt op het neveneffect van show/hide

- **regel 684**: WAT ER NIET STAAT: initPanels() zet buttonGameOverLabel wel op niet-tekenbaar maar disablet hem niet, terwijl hij op regel 407 al als touchlistener is aangemeld en Widget standaard met _bEnabled == true begint. Tussen initPanels() (aangeroepen op 928) en hidePage() (990) is hij dus onzichtbaar aanraakbaar. Vandaag en straks gelijk, maar het is de plek waar een expliciete disable(true) hoort - hetzelfde geldt voor buttonLeft/buttonRight.
- **regel 755**: OMGEKEERDE afhankelijkheid: buttonGameOverLabel.disable(true) wordt vandaag direct daarna ongedaan gemaakt door page.show(true) (757, via Widget::show -> enable) en nogmaals door page.enable(true) (758). Gevolg vandaag: het GAME OVER-vignet is tijdens het hele spel onzichtbaar maar wel touch-live (het is op regel 407 als touchlistener aangemeld en touchPressed kijkt alleen naar isEnabled()). Na de refactoring verdwijnt het enable-neveneffect van show, maar regel 758 blijft hem terugzetten - de disable moet dus NA page.enable(true) staan, anders verandert er niets.
- **regel 777**: page.hide(true) leunt op hide->disable voor de hele pagina (peddelknoppen en vignet). Gedekt: regel 778 doet page.disable(true) expliciet. Zodra 777 verdwijnt (dat moet, want de nieuwe hide() tekent) is 778 de enige uitschakeling van de spelpagina - hij mag dus niet als 'overbodig' worden opgeruimd.
- **regel 1032**: WAT ER NIET STAAT / wel goed staat: buttonLeft en buttonRight worden nergens anders geenabled dan hier (1032/1034) en via page.enable(true) op 758. PaddleControl gaat vanuit STATE_DISABLED altijd via showPaddle() (psup_PaddleControl.h regel 175), dus de peddelknoppen blijven ook zonder het show->enable-neveneffect bereikbaar. Deze pagina heeft dus GEEN plek waar een widget alleen door show() aanraakbaar werd zonder expliciete enable ernaast - het enige echte probleem zit in de volgorde op 755/758.
- **regel 1051**: paddlePanel.hide/buttonLeft.hide/buttonRight.hide (1051-1053) leunen op hide->disable. Gedekt: 1056-1058 disablen alle drie expliciet, en die staan met opzet BUITEN de if/else omdat de gebufferde tak (1045-1047) alleen setDrawable(false) doet en dus nooit iets uitschakelde. Dit is de bestaande, correcte workaround; hij moet blijven staan.
- **regel 1066**: OMGEKEERDE afhankelijkheid: pageGame.show(true) enablet vandaag de hele boom en zet daarmee buttonLeft/buttonRight/paddlePanel, die vlak daarvoor via flagHidePaddle op 1056-1058 zijn uitgeschakeld, weer aan. Tijdens het GAME OVER-vignet zijn de peddelknoppen dus onzichtbaar maar touch-live. Onschadelijk gebleken doordat PaddleControl in STATE_DISABLED flagLeftPressed/flagRightPressed weggooit bij de volgende enable (psup_PaddleControl.h regels 168-171), maar het is wel een gedragswijziging na de refactoring: ze blijven straks uit.
- **regel 1145**: OMGEKEERDE afhankelijkheid, 25x per seconde: pageGame.show(true) enablet elk beeld de hele boom opnieuw - inclusief buttonGameOverLabel (die op 755 en 1071 juist gedisabled wordt) en de peddelknoppen na een hidePaddle. Na de refactoring stopt dat. Dat is een verbetering, maar het betekent dat elke disable in dit bestand pas na de refactoring echt werkt; kijk bij het testen dus of er niets stiekem op 'wordt toch elk beeld weer aangezet' rekende.

## Observaties van de inventarisatie

SAMENVATTING PER RISICO: 24x compileerfout (uitsluitend setDrawable), 5x stil-gedrag (755, 757, 777, 1066, 1145), 13x mechanisch. Geen isShown(), redrawRegion(), repaintRegion() of paint() in dit bestand.

DE TWEE ECHTE VALKUILEN ZITTEN NIET IN show->enable MAAR IN show->makeDrawable.
Dit bestand gebruikt setDrawable(false) als 'zet dit slot even uit' voor 50 balpanels, 5 crates, 5 vuurkorven, de peddel, de twee peddelknoppen en het vignet, en tekent daarna de hele boom met page.show(true). Vandaag is dat veilig: Widget::show() raakt _bDrawable niet aan en Panel::showPanel() keert meteen terug als !isDrawable(). Met de nieuwe show() == makeDrawable + redraw zetten regels 1145 (elk beeld!) en 1066 alle uitgezette slots weer aan. Beide MOETEN redraw(true) worden. Hetzelfde geldt voor regel 757 in showPage(), waar show(true) anders het net uitgezette vignet (754) terugzet.

VOORGESTELDE NIEUWE showPage() (regel 747-759), die en de refactoring en de bestaande volgordefout oplost:
  page.makeDrawable(true);                  // eerst weer alles tekenbaar
  initBallAndPaddlePosition();              // zet ongebruikte slots juist weer uit
  buttonGameOverLabel.makeNonDrawable(true);// een nieuw spel begint zonder vignet
  page.redraw(true);                        // nu pas tekenen
  page.enable(true);                        // show() enablet niet meer
  buttonGameOverLabel.disable(true);        // NA page.enable, anders zet die hem terug aan
Vandaag staat page.setDrawable(true,true) NA initBallAndPaddlePosition(), waardoor bij het starten van een spel alle 50 balpanels op het veldmidden en alle crates/vuurkorven op (0,0) tekenbaar zijn en door page.show(true) ook echt getekend worden. Met een backbuffer is dat een beeld lang zichtbaar, zonder backbuffer blijft het staan. Het omdraaien van die twee regels is gratis meegenomen.

VOORGESTELDE NIEUWE hidePage() (regel 774-786):
  page.makeNonDrawable(true);
  page.disable(true);
  flagShowPaddle.clear(); flagHidePaddle.clear();
De hide()-aanroep moet er echt uit: het commentaar op 761-773 legt uit dat hidePage met opzet NIETS tekent omdat twee paginataken anders om beurten in dezelfde commandobuffer schrijven, en de nieuwe hide() doet juist een repaintRegion.

BUG DIE DE REFACTORING GRATIS REPAREERT (ongebufferde tak): op 1051-1053 zet hide() vandaag alleen _bVisible uit, niet _bDrawable. Een daaropvolgende pageGame.show(true) (1066) tekent de peddel en de peddelknoppen dus weer, terwijl ze net gewist waren. Met de nieuwe hide() == makeNonDrawable + repaint blijven ze weg.

BIBLIOTHEEK-AANNAMES WAAR DIT BESTAND OP LEUNT (controleer ze bij de refactoring van crt_Widget.h):
1. Widget::redrawRegion() breekt vandaag af op !_bVisible (crt_Widget.h regel 357) en Widget::moveTo() op !_bVisible (regel 394). Het commentaar op regel 741-746 van dit bestand ('redrawRegion() breekt af op een onzichtbare widget') gaat daarover. Zodra _bVisible verdwijnt moet die poort _bDrawable worden, anders (a) gaan moveTo() op 1095 en 1155 tekenen voor een pagina die uit beeld is, en (b) verliest hidePage() zijn bescherming tegen tekenen na het verbergen.
2. Panel::hide() (crt_Panel.h regel 226) is bewaakt met isShown(); die bewaking heeft na de refactoring een vervanger nodig, met dezelfde reden als in het commentaar op crt_Panel.h regel 215-223 (het wisvlak van de peddel viel bij het opstarten over de startknop van een andere taak).
3. TouchscreenButton::enable() (crt_TouchscreenButton.h 295-310) wist bPressed alleen bij een echte uit->aan-overgang, en het commentaar daar noemt expliciet 'Widget::show() roept enable(false) aan'. Dat commentaar veroudert met deze refactoring en de losse enable()-aanroepen (1032, 1034, 1065, 758) worden de enige bron van die overgang.

COMMENTAAR DAT MEE MOET VERANDEREN (het is uitleggend commentaar dat na de refactoring feitelijk onjuist wordt):
- regel 555-558: '... en daarna uitgezet. Een bal in het spel zetten is dan niet meer dan een positie en setDrawable(true)' -> makeDrawable.
- regel 710-712: 'Anders tekent show(true) ze nog even op de plek van het vorige spel' -> redraw(true).
- regel 737: '// zojuist verwerkt; show(true) tekent ze.' -> redraw.
- regel 741-746: de hele uitleg boven showPage() over show(true) op de PageRoot; herschrijven in termen van makeDrawable/redraw en het feit dat show nu ook drawable zet.
- regel 761-773: de uitleg boven hidePage() ('setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat') klopt straks niet meer - de nieuwe reden is dat hide() zelf repaint en daarom niet gebruikt mag worden.
- regel 1040-1044: 'setDrawable(false) en niet hide()' -> makeNonDrawable en niet hide(); de reden blijft geldig en wordt zelfs sterker.
- regel 1062-1063: 'Zelf doen dus' bij pageGame.show(true) -> redraw(true).

