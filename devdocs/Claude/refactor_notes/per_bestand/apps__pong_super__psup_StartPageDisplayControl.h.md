# Migratiebrief: `apps/pong_super/psup_StartPageDisplayControl.h`

10 call sites.

## Per call site

### regel 179 — `page.setDrawable(true, true);`  *(risico: compileerfout)*

**Bedoeling:** Eerste regel van de helper showPage(IWidget& page). De hele pagina-subtree weer tekenbaar zetten voordat er getekend wordt; hidePage() had hem non-drawable gemaakt. Het commentaar op regel 175-176 zegt expliciet dat dit dezelfde afspraak is als GameDisplayControl::showPage/hidePage, dus deze drie regels horen als geheel gemigreerd te worden.

**Wordt:** page.makeDrawable(true);  — of laat deze regel vervallen en laat de nieuwe show(true) op regel 180 het drawable-zetten doen (nieuwe show == makeDrawable + redraw). Alleen niet allebei weglaten.

### regel 180 — `page.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Nu daadwerkelijk de hele startpagina tekenen (background, titel, credits, playknop, twee fakkels). Regel 179 heeft drawable al gezet en regel 181 enablet expliciet, dus van de drie dingen die show() vandaag doet (_bVisible, enable, tekenen) is hier alleen het tekenen bedoeld.

**Wordt:** page.redraw(true);  — als je regel 179 laat staan is dit precies de bedoelde semantiek. Kies je ervoor regel 179 weg te laten, dan blijft page.show(true) staan (die zet dan zelf makeDrawable). Regel 181 blijft in beide gevallen nodig.

### regel 181 — `page.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina functioneel aanzetten: concreet maakt dit buttonStart weer aanraakbaar (TouchscreenButton::touchPressed/touchReleased keren meteen terug bij !isEnabled()). Vandaag is deze regel feitelijk dubbelop — show(true) op regel 180 enablet de hele subtree al — en leest hij als 'voor de duidelijkheid erbij'.

**Wordt:** ONGEWIJZIGD laten. Dit is na de refactoring de ENIGE plek die de startpagina (en dus de playknop) enablet. Niet opruimen als 'redundant': zonder deze regel blijft de knop dood na de eerste hidePage() op regel 219 en start het spel nooit meer.

### regel 186 — `page.setDrawable(false, true);`  *(risico: compileerfout)*

**Bedoeling:** Eerste regel van hidePage(). Bewust VOOR de hide(): zo slaat Panel::hide() zijn wisvlak over (Panel::hidePanel() keert terug bij !isDrawable()). Zie de uitgebreide toelichting bij psup_GameDisplayControl.h:761-773 waar deze afspraak staat: verbergen mag NIETS tekenen, omdat anders de wegtekenende taak en de taak van de volgende pagina in dezelfde commandobuffer om elkaar heen tekenen.

**Wordt:** page.makeNonDrawable(true);

### regel 187 — `page.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Bedoeld als: markeer de pagina als uit beeld, en teken daarbij niets. Dat 'niets' is geen toeval maar de kern van de opzet met drie tekentaken (zie regel 16-17 en psup_GameDisplayControl.h:761-773). Vandaag klopt dat: regel 186 heeft drawable al uitgezet, dus Panel::hide()/Text::hide() slaan hun fillRect over; wat overblijft is _bVisible=false plus de disable die regel 188 toch al expliciet doet.

**Wordt:** Veiligst: deze regel WEGHALEN. hidePage() wordt dan makeNonDrawable(true) + disable(true), en dat is exact de gedocumenteerde bedoeling. Laat je hem staan als hide(true), dan hangt het no-draw-contract er volledig van af dat de nieuwe hide() éérst makeNonDrawable doet en pas daarna repaintRegion, én dat redrawRegion() straks op _bDrawable afslaat in plaats van op het verdwenen _bVisible (crt_Widget.h:356). Bovendien kost hide(true) op een root met 6 kinderen dan 6 volledige root-traversals plus setClipRect/clearClipRect per stuk, voor nul pixels.

### regel 188 — `page.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** De pagina functioneel uitzetten: buttonStart mag niet meer reageren zolang het spel of de game-over-pagina in beeld staat. Vandaag dubbelop (hide(true) op regel 187 disablet al), en TouchscreenButton::disable() gooit hier bovendien bPressed weg — precies het geval dat in crt_TouchscreenButton.h:271-278 beschreven staat: knop drukt zichzelf weg, vinger komt in het niets omhoog.

**Wordt:** ONGEWIJZIGD laten. Dit wordt de enige disable van de startpagina. Weghalen betekent: playknop blijft het hele spel door aanraakbaar terwijl hij onzichtbaar is, op dezelfde schermplek waar peddel-touch en de game-over-knop zitten.

### regel 219 — `hidePage(pageStart);	// StartPageControl vraagt hem zo meteen zelf op.`  *(risico: mechanisch)*

**Bedoeling:** Direct na het opbouwen van de boom: begin verborgen, StartPageControl vraagt de pagina zo zelf op. Dit is ook het moment waarop buttonStart voor het eerst gedisabled raakt — hij is al sinds de constructor (regel 96, display.addTouchListener(&buttonStart)) een touchlistener en Widget's constructor zet _bEnabled op TRUE. Zonder de disable(true) uit hidePage zou de knop dus vanaf boot aanraakbaar zijn.

**Wordt:** Geen wijziging aan deze regel; het gedrag zit in hidePage(). Wel controleren dat hidePage zijn disable(true) houdt (regel 188), anders verandert juist hier stil het gedrag. Let ook op Panel::hide()'s isShown()-guard (crt_Panel.h:224-231), die er specifiek is voor deze aanroep: een pagina die nog nooit een pixel zette moest bij het verbergen niet alsnog zijn vlak vullen. Die guard kan met _bVisible mee verdwijnen, mits hidePage het non-drawable-eerst-patroon houdt.

### regel 231 — `showPage(pageStart);`  *(risico: mechanisch)*

**Bedoeling:** STATE_HIDDEN -> STATE_SHOWING: de startpagina in beeld brengen en de fakkeltimer starten. Alles wat er functioneel moet gebeuren (drawable, tekenen, enablen) is gedelegeerd naar showPage() op regel 177-182.

**Wordt:** Geen wijziging aan deze regel.

### regel 245 — `hidePage(pageStart);`  *(risico: mechanisch)*

**Bedoeling:** STATE_SHOWING -> STATE_HIDDEN nadat flagHideStartPage is gezet (StartPageControl::doStartPage regel 68, na de druk op PLAY). Fakkeltimer eerst stoppen (regel 244), dan de pagina weg. Hier is de disable functioneel belangrijk: de knop die zichzelf net heeft weggedrukt moet ook echt stil worden, en zijn bPressed moet gewist zodat de volgende ronde weer een buttonPressed oplevert.

**Wordt:** Geen wijziging aan deze regel; gedrag zit in hidePage(). De disable(true) op regel 188 moet blijven, anders verdwijnt de bPressed-reset van TouchscreenButton::disable() en is de knop na één spel voorgoed 'ingedrukt'.

### regel 251 — `pageStart.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Puur hertekenen. Na selectNextAnimationFrameOfTorches() (regel 250) staat er een nieuwe cel in beide fakkelpanelen en moet de pagina opnieuw op het scherm; het commentaar op regel 233-234 zegt het letterlijk: 'elke tik is een volledige hertekening'. Van drawable/enable/visible is hier niets bedoeld — die staan al goed sinds regel 231. Deze aanroep vuurt 8 tot 22 keer per seconde (TorchFrameIntervalMinMs 45 .. MaxMs 130).

**Wordt:** pageStart.redraw(true);

## Leunt op het neveneffect van show/hide

- **regel 181**: GOED NIEUWS, met een valkuil: showPage() leunt NIET op show->enable, want regel 181 doet page.enable(true) al expliciet. Precies daardoor is deze helper immuun voor de stille gedragswijziging. De valkuil is de omgekeerde: onder de oude API is regel 181 zichtbaar overbodig (show(true) enablet de hele subtree al), dus een opruimende refactoring die 179+180+181 samentrekt tot 'page.show(true)' - want show == makeDrawable + redraw - haalt de enige enable weg. Gevolg: buttonStart blijft na de eerste hidePage() op regel 219 voorgoed disabled, touchPressed keert meteen terug, MainControl blijft eeuwig in STATE_START_PAGE hangen op wait(flagStartPressed). Geen compileerfout, geen logregel, alleen een dood scherm.
- **regel 188**: Spiegelbeeld van regel 181: hidePage() leunt NIET op hide->disable, want regel 188 doet page.disable(true) expliciet. Ook hier is de regel vandaag zichtbaar dubbelop en dus kandidaat om 'meegenomen' te worden bij het herschrijven van hide(). Weghalen betekent: de playknop blijft het hele spel en de hele game-over-pagina door aanraakbaar (hij blijft touchlistener, zie regel 96 - alleen _bEnabled schakelt hem uit), op de rect y=155..209 waar ook de peddel-touch en de PLAY AGAIN-knop van GameOverDisplayControl zitten. Bovendien vervalt dan de bPressed-reset uit TouchscreenButton::disable(), waardoor de knop die zichzelf wegdrukt met bPressed==true blijft hangen en bij de volgende ronde nooit meer een buttonPressed stuurt (het scenario dat crt_TouchscreenButton.h:271-278 beschrijft).
- **regel 251**: Dit is de enige plek in dit bestand waar het show->enable-neveneffect herhaaldelijk vuurt: pageStart.show(true) enablet 8-22x per seconde de hele subtree opnieuw. Nagegaan of iets daarop leunt: nee. Niemand disablet buttonStart tijdens STATE_SHOWING - psup_StartPageControl.h gebruikt de IButton* alleen voor de identiteitsvergelijking in buttonPressed(), en er staat nergens in de app een enable/disable op de startknop buiten showPage/hidePage. De conversie naar redraw(true) is dus gedragsbehoudend. Wel is deze regel de veroorzaker van de workaround in de bibliotheek: TouchscreenButton::enable() (crt_TouchscreenButton.h:295-308) wist bPressed alleen bij een echte uit->aan-overgang, met als toelichting 'Dat laatste gebeurde wel, want Widget::show() roept enable(false) aan' - een druk die op dit moment loopt werd anders elke fakkeltik weggegooid en de knop bleef verzonken staan. Zodra show() niet meer enablet, kan die guard weg.

## Observaties van de inventarisatie

SAMENVATTING: 10 aanroepen, in 4 blokken. Twee harde compileerfouten (regel 179 en 186, setDrawable). Geen enkele isShown() in dit bestand. Geen enkele redrawRegion(, repaintRegion( of paint( in dit bestand.

HET BELANGRIJKSTE: dit bestand leunt NERGENS op het neveneffect. showPage() (regel 177-182) en hidePage() (regel 184-189) zetten enable respectievelijk disable er al expliciet bij. Het risico zit hier dus niet in wat er staat maar in wat een opruimende refactoring ermee doet: regel 181 en 188 LIJKEN onder de oude API overbodig en zijn straks de enige plek die de startpagina aan- en uitzet. Laat ze staan.

VOORGESTELDE MIGRATIE (compleet):
  void showPage(IWidget& page)          void hidePage(IWidget& page)
  {                                     {
      page.makeDrawable(true);              page.makeNonDrawable(true);
      page.redraw(true);                    page.disable(true);
      page.enable(true);                }
  }
en op regel 251: pageStart.redraw(true);
In hidePage vervalt page.hide(true) dus helemaal. Reden: de nieuwe hide() == makeNonDrawable + repaintRegion(eigen rect), en repaintRegion is precies wat hier NIET mag gebeuren. Het commentaar op regel 16-17 van dit bestand en de lange toelichting bij psup_GameDisplayControl.h:761-773 leggen uit waarom: drie tekentaken delen een commandobuffer, dus als de vertrekkende pagina een zwart vlak wegschrijft terwijl de aankomende al tekent, wist het ene beeld het andere. Werkt alleen omdat elke pagina met een schermvullend backgroundPanel begint (regel 53-55).
Laat je hide(true) toch staan, dan hangt het no-draw-contract aan twee dingen die buiten dit bestand liggen: (a) de nieuwe hide() moet éérst makeNonDrawable doen en pas daarna repaintRegion, en (b) Widget::redrawRegion() (crt_Widget.h:355-359) draait nu op 'if (!_bVisible) return;' en moet straks op _bDrawable afslaan - anders is de guard weg en tekent de hele boom alsnog.

WAT ER NIET STAAT (bewust gecontroleerd):
- Er staat nergens een enable()/disable() op buttonStart zelf. De knop is touchlistener vanaf de constructor (regel 96) en Widget's ctor zet _bEnabled=true, dus tussen start() (regel 98) en hidePage() (regel 219) is hij aanraakbaar terwijl er niets in beeld staat. Dat is bestaand gedrag en verandert niet door de refactoring - mits regel 188 blijft.
- Er staat geen enable/disable rond de fakkeltik (regel 248-256). Dat hoeft ook niet zodra show() niet meer enablet; vandaag draagt die tik het enablen ongewild mee.
- Er staat geen setDrawable per widget (anders dan via de page), geen enkel widget wordt hier apart in/uit beeld gezet. Contrast met psup_GameOverDisplayControl.h:373/391 (buttonStart.setDrawable(!bName,true) + buttonStart.enable(!bName)) - daar is de knop wel apart geschakeld; die kant is niet mijn opdracht maar wel hetzelfde patroon.

BESTAANDE WORKAROUNDS DIE STRAKS WEG KUNNEN (alle in de bibliotheek, veroorzaakt door dit gebruikspatroon):
- crt_TouchscreenButton.h:295-308, TouchscreenButton::enable(): de guard 'alleen wissen bij echte uit->aan-overgang', met de toelichting dat Widget::show() enable(false) aanroept. Overbodig zodra show() niet meer enablet.
- crt_TouchscreenButton.h:271-278: de slotzin 'Widget::hide() disabelt ook, dus dit dekt hide() mee' wordt onwaar - de bPressed-reset komt dan alleen nog via de expliciete disable(true) op regel 188.
- crt_Panel.h:224-231 (Panel::hide met 'if (WBase::isShown())') en crt_Text.h:194-196 (idem): die isShown()-guard bestaat specifiek voor de hidePage() op regel 219 van dit bestand - een pagina die nog nooit een pixel zette mocht bij het verbergen niet alsnog zijn vlak vullen (het commentaar noemt de peddel-wisvlak-over-de-startknop-bug bij het opstarten). Verdwijnt met _bVisible.
- crt_Widget.h:355-359 (redrawRegion) en crt_Widget.h:392 (moveTo) lezen _bVisible; die moeten mee omgebouwd worden naar _bDrawable.

