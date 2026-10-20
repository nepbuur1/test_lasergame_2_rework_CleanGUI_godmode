# Migratiebrief: `apps/pong_super/psup_GameOverDisplayControl.h`

25 call sites.

## Per call site

### regel 362 — `hallOfFameTitlePanel.setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Fasewissel puur op de vlag. Het commentaar op r.350-353 zegt het met zoveel woorden: 'setDrawable en niet hide(): hide() zou een wisvlak insturen, en dat is bij een pagina die als geheel opnieuw wordt getekend zowel overbodig als riskant'. De titel hoort bij de LIJST-fase; in de naamfase staat het invoerveld op die hoogte. Er mag hier dus niets getekend en niets gewist worden.

**Wordt:** setDrawable(bool,bool) verdwijnt en de waarde is hier een runtime-bool, dus geen 1-op-1 vervanging. Voeg een file-locale helper toe: `static void setDrawableFlag(IWidget& w, bool b) { if (b) { w.makeDrawable(true); } else { w.makeNonDrawable(true); } }` en schrijf `setDrawableFlag(hallOfFameTitlePanel, !bName);`. Semantiek is identiek: makeDrawable/makeNonDrawable tekenen en wissen niets.

### regel 364 — `typeYourNameText.setDrawable(bName, true);`  *(risico: compileerfout)*

**Bedoeling:** 'TYPE UW NAAM' hoort alleen bij de naamfase. Alleen de vlag, geen tekenwerk (zie r.350-353).

**Wordt:** setDrawableFlag(typeYourNameText, bName);

### regel 365 — `inputfield.setDrawable(bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Het invoerveld hoort alleen bij de naamfase; in de lijstfase moet het onzichtbaar zijn maar wel in de boom blijven staan.

**Wordt:** setDrawableFlag(inputfield, bName);

### regel 366 — `touchscreenKeyboard.setDrawable(bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Het toetsenbord (incl. zijn 40 toetsknoppen — vandaar bIncludeChildren=true) alleen tekenbaar in de naamfase.

**Wordt:** setDrawableFlag(touchscreenKeyboard, bName);

### regel 370 — `arHallOfFameNames[i].setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** De naamkolom van de hall of fame hoort bij de lijstfase (NofHallOfFameEntries = 5, dus 5 aanroepen per doorloop).

**Wordt:** setDrawableFlag(arHallOfFameNames[i], !bName);

### regel 371 — `arHallOfFameScores[i].setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Idem voor de scorekolom.

**Wordt:** setDrawableFlag(arHallOfFameScores[i], !bName);

### regel 373 — `buttonStart.setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** De 'Play Again'-knop hoort bij de lijstfase. Let op: dit zet alleen de tekenbaarheid — NIET de aanraakbaarheid. Zie r.391.

**Wordt:** setDrawableFlag(buttonStart, !bName);

### regel 374 — `arTorchPanels[0].setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Fakkel links naast de knop. Commentaar r.147-148: 'Ze horen bij de lijstfase; tijdens het intypen staan ze uit.'

**Wordt:** setDrawableFlag(arTorchPanels[0], !bName);

### regel 375 — `arTorchPanels[1].setDrawable(!bName, true);`  *(risico: compileerfout)*

**Bedoeling:** Fakkel rechts, idem.

**Wordt:** setDrawableFlag(arTorchPanels[1], !bName);

### regel 389 — `inputfield.enable(bName);`  *(risico: stil-gedrag)*

**Bedoeling:** Blijkens de functienaam drawAndEnableWidgetsForPhase en de symmetrie met r.390-391 bedoelt de programmeur: 'aanraakbaar in de naamfase, niet in de lijstfase'. Dat is NIET wat er staat: de parameter van enable() is bIncludeChildren, niet de gewenste toestand. Widget::enable() zet _bEnabled altijd op true. Het veld wordt dus in beide fasen ingeschakeld; alleen het doorgeven aan kinderen varieert.

**Wordt:** Echte enable/disable maken: `if (bName) { inputfield.enable(true); } else { inputfield.disable(true); }`. De nieuwe API verandert enable/disable niet, maar dit is de plek waar het uitzetten hoort te gebeuren zodra show() het niet meer overschrijft.

### regel 390 — `touchscreenKeyboard.enable(bName && bKeyboardIsUp);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeld: toetsenbord alleen aanraakbaar als het én de naamfase is én het toetsenbord daadwerkelijk omhoog staat. Feitelijk: het toetsenbord wordt altijd ingeschakeld, en alleen bij (bName && bKeyboardIsUp) worden ook de 40 toetsknoppen meegenomen.

**Wordt:** `if (bName && bKeyboardIsUp) { touchscreenKeyboard.enable(true); } else { touchscreenKeyboard.disable(true); }`

### regel 391 — `buttonStart.enable(!bName);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeld: 'Play Again' alleen aanraakbaar in de lijstfase. Feitelijk blijft de knop óók tijdens het intypen ingeschakeld — en hij ligt op y=176, midden onder het opgeschoven toetsenbord (rotatie 1 -> 320x240; toetsenbord omhoog = y 120..240). Een toetsaanslag in de onderste rijen levert dus tegelijk een buttonPressed op de onzichtbare knop. Dat is vandaag nog onschadelijk omdat GameOverPageControl::doGameOver() na showHallOfFame() flagStartPressed.clear() doet.

**Wordt:** `if (bName) { buttonStart.disable(true); } else { buttonStart.enable(true); }` — disable(true) wist bovendien bPressed (TouchscreenButton::disable), wat precies is wat je wilt bij een knop die onder het toetsenbord verdwijnt.

### regel 461 — `touchscreenKeyboard.disable(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Tijdens het in-/uitschuiven mag het toetsenbord geen aanslagen aannemen. De aanroep is correct bedoeld, maar wordt vandaag onmiddellijk weer ongedaan gemaakt door page.show(true) op r.483, die elke animatiestap de hele boom opnieuw enablet. Feitelijk zijn de toetsen dus wél live tijdens de animatie.

**Wordt:** Ongewijzigd laten. Zodra show() niet meer enablet, doet deze disable eindelijk wat er staat — een stille gedragswijziging in de goede richting (geen toetsaanslagen meer halverwege de schuifbeweging).

### regel 483 — `page.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Per animatiestap de hele pagina opnieuw tekenen. Het commentaar r.479-481 zegt waarom: 'Met een backbuffer verzet moveTo() alleen de coordinaten van een deel van de boom; de hele pagina moet daarna opnieuw.' Bedoeld is dus zuiver TEKENEN — niet enablen en zeker niet drawable maken.

**Wordt:** page.redraw(true); — LET OP: show(true) wordt hier ronduit fout. show == makeDrawable + redraw, en makeDrawable(true) op de PageRoot zet alle 32 kinderen weer tekenbaar. Daarmee sneuvelt de hele fasescheiding van r.362-375 en worden hall of fame én naaminvoer over elkaar heen getekend.

### regel 497 — `touchscreenKeyboard.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Na een geslaagde schuifbeweging omhoog het toetsenbord weer aanraakbaar maken, inclusief zijn toetsknoppen (bIncludeChildren=true). Vandaag overbodig — page.show(true) op r.483 heeft alles al geenabled — maar wel de enige plek die de toetsen ná de disable van r.461 recursief terugzet.

**Wordt:** Ongewijzigd. Deze regel wordt van overbodig naar dragend: na de refactoring is dit de enige aanroep die de 40 toetsknoppen weer inschakelt. Niet weghalen bij het opruimen.

### regel 505 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** De hele pagina weer tekenbaar zetten voordat de fase-selectie er op r.506 gaten in prikt. Vaste waarde true, dus geen helper nodig.

**Wordt:** page.makeDrawable(true);

### regel 507 — `page.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De pagina in beeld zetten: teken alles wat na r.505-506 nog tekenbaar is. Het commentaar op r.501-502 verwijst naar GameDisplayControl::showPage, waar dezelfde volgorde staat (setDrawable, show, enable). Bedoeld is 'teken nu'.

**Wordt:** page.redraw(true); — NIET show(true) laten staan: die zou via makeDrawable(true) de setDrawable(false) van r.506 (drawAndEnableWidgetsForPhase) meteen weer ongedaan maken, waarna beide fasen tegelijk op het scherm staan.

### regel 508 — `page.enable(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De hele pagina weer aanraakbaar maken na hidePage(). Vandaag dubbelop (show() enablet al), na de refactoring de enige bron van enable — en dus goed dat hij er staat. Maar hij staat NA r.506 en overschrijft daarmee alles wat drawAndEnableWidgetsForPhase per fase had willen uitzetten.

**Wordt:** Ongewijzigd laten, maar verplaats hem vóór de aanroep van drawAndEnableWidgetsForPhase: `page.makeDrawable(true); page.enable(true); drawAndEnableWidgetsForPhase(phase); page.redraw(true);`. Anders blijft de fase-enable van r.389-391 zinloos, ook nadat die op echte enable/disable is omgezet.

### regel 513 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Eerst niet-tekenbaar maken, zodat de hide() daaronder gegarandeerd niets tekent. Zie het uitgebreide commentaar bij GameDisplayControl::hidePage (r.761-772 daar): een wissende hide zet een zwart vlak in de commandobuffer terwijl de volgende pagina er al in schrijft, en welke er als laatste uitkomt ligt niet vast.

**Wordt:** page.makeNonDrawable(true);

### regel 514 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeld als 'zet de _bVisible-vlag om en disable, maar teken niets' — het niet-tekenen wordt afgedwongen door de setDrawable(false) op r.513 (Panel::hidePanel/Text::hideText keren meteen terug bij !isDrawable). Zie ook de kop van het bestand, r.24: 'Zie psup_GameDisplayControl.h voor waarom het verbergen niets tekent.'

**Wordt:** HELEMAAL WEGHALEN. In de nieuwe API is hide == makeNonDrawable + repaintRegion(eigen rect), dus juist wél tekenwerk — precies wat deze opzet met drie tekentaken niet mag. Wat overblijft: `page.makeNonDrawable(true); page.disable(true);`. (Ook zonder zichtbaar resultaat kost hide(true) dan een clipRect + volledige boomwandeling per widget.)

### regel 515 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De hele pagina doof maken zodra hij uit beeld is. Vandaag dubbelop met de disable die in hide() zit; na de refactoring is dit de enige bron.

**Wordt:** Ongewijzigd — en beslist niet 'opruimen als dubbelop'. Dit is de regel die hidePage na de refactoring correct houdt.

### regel 573 — `pageGameOver.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Fakkeltik: na TorchesDisplayHelpers::stepTorches() de hele pagina opnieuw tekenen (backbuffer, dus deelupdate kan niet). Commentaar r.376-379: 'elke tik is een volledige hertekening'. Bedoeld is zuiver TEKENEN.

**Wordt:** pageGameOver.redraw(true); — show(true) zou hier via makeDrawable de fase-vlaggen wissen en bij elke fakkeltik het invoerveld en het toetsenbord over de hall of fame heen zetten.

### regel 609 — `pageGameOver.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De naamfase in beeld zetten, direct na drawAndEnableWidgetsForPhase(PHASE_NAME_INPUT) op r.608. Bedoeld: teken de pagina met de zojuist gekozen fase-vlaggen.

**Wordt:** pageGameOver.redraw(true); — met show(true) worden de op r.362-375 net uitgezette lijst-widgets (titel, 10 Texts, knop, 2 fakkels) meteen weer tekenbaar en dus over het invoerveld heen getekend.

### regel 625 — `pageGameOver.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De lijstfase in beeld zetten na fillHallOfFameLines() en drawAndEnableWidgetsForPhase(PHASE_HALLOFFAME). Bedoeld: teken de pagina opnieuw met de nieuwe teksten en posities.

**Wordt:** pageGameOver.redraw(true); — met show(true) worden typeYourNameText, inputfield en het toetsenbord weer tekenbaar en over de hall of fame heen getekend.

### regel 639 — `pageGameOver.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Per toetsaanslag hertekenen. Commentaar r.632-636: 'Elke andere toets verandert het veld wel, en dan moet met een backbuffer de hele pagina opnieuw.' Zuiver tekenen; ENTER wordt bewust overgeslagen omdat er dan niets verandert.

**Wordt:** pageGameOver.redraw(true); — anders wordt bij elke ingetypte letter de hall of fame weer tekenbaar en over het toetsenbord heen gezet.

## Leunt op het neveneffect van show/hide

- **regel 483**: page.show(true) in slideKeyboard() maakt de touchscreenKeyboard.disable(true) van r.461 elke animatiestap weer ongedaan (show -> enable(false) per knoop, recursief via de kinderen). De toetsen zijn vandaag dus live tijdens de hele schuifbeweging; alleen r.494-498 herstelt de bedoelde eindtoestand. Na de refactoring blijft de disable staan — gewenst, maar wel een stille gedragswijziging.
- **regel 507**: page.show(true) in showPage() enablet vandaag de hele boom (alle 32 kinderen, inclusief de 40 toetsknoppen). Dat neveneffect valt weg, maar wordt hier opgevangen door de expliciete page.enable(true) op r.508 — showPage breekt dus niet. Wel is de combinatie 507+508 de reden dat de fase-enable van r.389-391 vandaag geen enkel effect heeft: beide zetten alles weer aan ná r.506.
- **regel 514**: page.hide(true) in hidePage() disablet vandaag de hele boom. Dat neveneffect valt weg, maar wordt opgevangen door de expliciete page.disable(true) op r.515. Het echte probleem bij deze regel is de andere kant: hide() gaat juist WEL tekenen (repaintRegion), terwijl de hele opzet met drie tekentaken erop rust dat verbergen niets tekent (r.24 en het commentaar bij GameDisplayControl::hidePage).
- **regel 573**: pageGameOver.show(true) bij elke fakkeltik enablet vandaag de hele pagina opnieuw, tientallen keren per seconde. Elke disable die elders is gezet (r.461 op het toetsenbord, of een disable in drawAndEnableWidgetsForPhase als die wordt gerepareerd) wordt daardoor binnen één fakkeltik weggevaagd. Na de refactoring blijft de disable staan — dat maakt r.389-391 pas werkelijk effectief, en is meteen de reden dat die regels goed gezet moeten worden.
- **regel 609**: Wat er NIET staat: bij het ingaan van de naamfase enablet niets in deze functie de 40 toetsknoppen recursief. r.390 doet touchscreenKeyboard.enable(bName && bKeyboardIsUp), en op dat moment is bKeyboardIsUp nog false, dus bIncludeChildren is false. Vandaag redt pageGameOver.show(true) op r.609 (en r.483) dat door alles te enablen; morgen hangt het volledig aan touchscreenKeyboard.enable(true) op r.497, aan het eind van de schuifbeweging. Zolang r.611 de slide uitvoert werkt dat. Maar wordt de slide overgeslagen (bKeyboardIsUp al true bij binnenkomst van flagShowNameInput), dan blijven de toetsen dood bij een puur mechanische migratie. De voorgestelde reparatie van r.390 (echte enable(true)/disable(true)) dekt ook dat pad af.
- **regel 625**: Wat er NIET staat: nergens in dit bestand wordt inputfield.disable(...) aangeroepen. Bij het teruggaan naar de lijstfase blijft het invoerveld dus aanraakbaar (r.389 enablet altijd, en show() bevestigt dat vandaag). Het veld staat op x 32..288 / y 60..96 (locPos 100,250 promillage van 320x240, size 800x150 promillage), en de hall-of-fame-regels beginnen op HallOfFameTopPix = 70 — de bovenste twee regels van de lijst liggen dus onder een onzichtbaar, live invoerveld. Onschadelijk zolang inputfieldPressed() in beide luisteraars leeg is (deze klasse r.264-268 en NameInputControl r.95-99), maar het is precies het soort stille afhankelijkheid dat nu moet worden vastgelegd met een expliciete disable.

## Observaties van de inventarisatie

TOTALEN: 25 aanroepen op widgets — 11x setDrawable, 6x show, 1x hide, 4x enable, 2x disable, 1x page.enable/page.disable meegerekend in die vier/twee. GEEN enkele aanroep van isShown(), redrawRegion(), repaintRegion() of paint() in dit bestand. Verder nog drie indirecte aanroepen via de eigen wrappers: r.561 hidePage(pageGameOver) (bij het opstarten, met commentaar 'bij het eerste opstarten is er nog niets verloren'), r.585 hidePage(pageGameOver) en r.592 showPage(pageGameOver). Die hoeven zelf niet te veranderen.

HET GROOTSTE GEVAAR IN DIT BESTAND (5 van de 6 show-aanroepen): deze PageRoot draagt TWEE fasen tegelijk. Het commentaar op r.32-34 zegt het expliciet: 'Twee fasen op een PageRoot en niet twee PageRoots ... Wisselen gaat met setDrawable().' Vandaag is page.show(true) veilig omdat show() _bDrawable niet aanraakt: de niet-tekenbare fase slaat zijn tekenwerk over in Panel::showPanel()/Text::showText() (die keren meteen terug bij !isDrawable()). In de nieuwe API is show == makeDrawable + redraw, dus show(true) op de root zet alle 32 kinderen tekenbaar en tekent BEIDE fasen over elkaar heen. Alle zes de show-aanroepen (r.483, 507, 573, 609, 625, 639) bedoelen zonder uitzondering 'teken nu' en moeten redraw(true) worden. Ter contrast staat in psup_GameDisplayControl.h r.740-745 het commentaar dat show(true) op de root dáár juist wél mag, 'want deze root draagt maar een pagina. (In een opzet met drie pagina's onder een gedeelde root zou show(true) ze alle drie over elkaar heen tekenen.)' — dit bestand is precies dat geval.

TWEEDE GEVAAR: hidePage() op r.511-516 mag niets tekenen. Dat is een expliciete afspraak (r.24 in dit bestand, en het lange commentaar bij GameDisplayControl::hidePage r.761-772: een wissende hide zet een zwart vlak in de commandobuffer terwijl de volgende pagina er al in schrijft). De nieuwe hide() = makeNonDrawable + repaintRegion doet juist wél iets. Met de hele pagina al niet-tekenbaar komt er weliswaar geen pixel uit, maar er gaat wel een setClipRect + volledige boomwandeling doorheen (en bij bIncludeChildren mogelijk 33 keer). hide() moet hier gewoon verdwijnen: makeNonDrawable(true) + disable(true) is de volledige bedoeling.

BESTAANDE 'WORKAROUNDS' DIE JUIST MOETEN BLIJVEN: r.508 page.enable(true) en r.515 page.disable(true) staan er als dubbelop naast show()/hide(). Ze zijn vandaag overbodig en worden morgen dragend — niet opruimen. Idem r.497 touchscreenKeyboard.enable(true). Dat geldt ook voor het vergelijkbare paar in showPage/hidePage van GameDisplayControl.

LATENTE FOUT DIE NU AAN HET LICHT KOMT (r.389-391): de programmeur schrijft enable(bName) / enable(bName && bKeyboardIsUp) / enable(!bName) alsof de parameter de gewenste toestand is. Dat is hij niet — Widget::enable(bool bIncludeChildren) zet _bEnabled altijd op true (crt_Widget.h r.556-566). Alle drie de widgets worden dus in beide fasen ingeschakeld. Concreet gevolg vandaag: tijdens het intypen is buttonStart ('Play Again', y=176) aanraakbaar onder het opgeschoven toetsenbord (dat bij rotatie 1 / 320x240 y 120..240 beslaat). Een aanslag op de onderste toetsrijen levert dus tegelijk een buttonPressed op. Dat blijft nu net onschadelijk doordat GameOverPageControl::doGameOver() ná showHallOfFame() flagStartPressed.clear() aanroept. De refactoring is het natuurlijke moment om dit recht te zetten, want zodra show() niet meer enablet is drawAndEnableWidgetsForPhase() de enige plek die de aanraakbaarheid per fase bepaalt.

AANBEVOLEN EINDVORM VAN showPage() (r.503-509), waarin de enable-volgorde meteen wordt rechtgezet:
  page.makeDrawable(true);
  page.enable(true);                       // eerst de hele pagina aan ...
  drawAndEnableWidgetsForPhase(phase);     // ... daarna uitzetten wat niet bij deze fase hoort
  page.redraw(true);
en van hidePage() (r.511-516):
  page.makeNonDrawable(true);
  page.disable(true);

BIBLIOTHEEKKANT, ter info voor de refactoring zelf: TouchscreenButton::enable() (crt_TouchscreenButton.h r.295-308) bevat een guard die uitsluitend bestaat omdat Widget::show() enable(false) aanroept ('Dat laatste gebeurde wel, want Widget::show() roept enable(false) aan'). Ook TouchscreenButton::disable() r.270-283 noemt 'Widget::hide() disabelt ook, dus dit dekt hide() mee' — die zin wordt onwaar. Widget::redrawRegion() (crt_Widget.h r.355-358) en Widget::moveTo() (r.394) toetsen nu op _bVisible; die moeten op _bDrawable over. Panel::hide()/Text::hide() toetsen op isShown() (crt_Panel.h r.226, crt_Text.h r.196) — dat zijn de echte compileerfouten van het isShown-verwijderen, niet dit app-bestand.

