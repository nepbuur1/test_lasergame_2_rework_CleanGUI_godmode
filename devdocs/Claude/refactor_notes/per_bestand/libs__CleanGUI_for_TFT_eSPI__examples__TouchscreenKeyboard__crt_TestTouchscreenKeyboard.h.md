# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenKeyboard/crt_TestTouchscreenKeyboard.h`

4 call sites.

## Per call site

### regel 156 — `page1.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Teken de hele pagina in een keer (achtergrondpaneel onderop plus het toetsenbord); het commentaar op 152-155 zegt expliciet 'de hele pagina, niet alleen het toetsenbord', anders wordt het achtergrondpaneel nooit getekend.

**Wordt:** page1.show(true); blijft letterlijk staan (= makeDrawable + redraw). Een extra enable is hier niet nodig: _bEnabled staat na constructie al op true en er is nergens eerder gedisabled. Wie alleen tekenwerk bedoelt kan page1.redraw(true) gebruiken, maar dan blijft de hide() op regel 180 het toetsenbord non-drawable maken en dat is hier ongewenst.

### regel 180 — `touchscreenKeyboard.hide(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Reactie op de pseudo-toets 'OutsideKeyboardPressed': er is buiten het toetsenbord getikt, dus het toetsenbord moet weg. Het moet daarna ook echt uit staan — de test verwacht dat de volgende aanraking alleen nog flagTouchPressed zet (tak op 203) en geen toetsaanslagen meer produceert.

**Wordt:** touchscreenKeyboard.hide(true); gevolgd door touchscreenKeyboard.disable(true); — precies in die volgorde, zodat eerst het beeld wordt hersteld en daarna de toetsen dood zijn. disable(true) is verplicht met children: hide(true) disabelde nu ook alle 32 knoppen plus tsOutsideKeyboardButton.

### regel 207 — `if(!touchscreenKeyboard.isShown())`  *(risico: compileerfout)*

**Bedoeling:** Alleen opnieuw tonen als het toetsenbord nu verborgen is; het commentaar op 205-206 legt uit dat de vorige aanraking het toetsenbord had verborgen en dat de test het 'for the sake of this test' weer terugzet.

**Wordt:** if(!touchscreenKeyboard.isDrawable()) — dat is de directe vervanger, want de nieuwe hide() zet _bDrawable op false en show() weer op true. Alternatief, en eerlijker zodra hide/disable altijd samen gaan: !touchscreenKeyboard.isEnabled(), of een eigen bool bKeyboardShown in de taak. Let op het verschil in beginwaarde: isShown() was false tot de eerste show(), isDrawable() is true vanaf constructie — hier onschuldig, want dit punt wordt pas na page1.show(true) bereikt.

### regel 209 — `touchscreenKeyboard.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Het verborgen toetsenbord weer terugzetten na een aanraking buiten het toetsenbord — en het moet daarna weer aanraakbaar zijn, anders is de test na één ronde stuk: er komt nooit meer een keyPressed.

**Wordt:** touchscreenKeyboard.show(true); gevolgd door touchscreenKeyboard.enable(true); (children mee: hide(true) had ze allemaal uitgezet). enable ná show, zodat er geen aanraking op een half getekend toetsenbord binnenkomt; de bPressed-reset in TouchscreenButton::enable() vangt de overgang uit->aan nog steeds af.

## Leunt op het neveneffect van show/hide

- **regel 180**: hide(true) is hier de enige plek die het toetsenbord uitzet; er staat nergens disable(). Aanraakafhandeling kijkt uitsluitend naar _bEnabled (TouchscreenButton::touchPressed regel 219, TouchscreenButtonGroup::touchPressed regel 77) en niet naar _bVisible, dus zonder de disable blijft het onzichtbare toetsenbord toetsen versturen. Erger: de if/else-keten op 173-211 neemt dan altijd de queueKeyPressedStrings-tak, waardoor de flagTouchPressed-tak op 203 nooit meer draait en het toetsenbord nooit meer terugkomt. Bovendien blijft tsOutsideKeyboardButton vuren, zodat elke aanraking opnieuw 'OutsideKeyboardPressed' oplevert.
- **regel 209**: show(true) is de enige plek die het toetsenbord na de hide van regel 180 weer aan zet; er staat nergens enable(). Zonder de enable-kant van het neveneffect komt het toetsenbord wel terug in beeld maar blijft het dood: TouchscreenButtonGroup::touchPressed valt op !isEnabled() meteen terug en er komt nooit meer een keyPressed, dus de tekst-uitvoer op regel 194 werkt na de eerste hide niet meer.

## Observaties van de inventarisatie

In alle vier de voorbeelden staat _bEnabled al vanaf de constructor van Widget op true (crt_Widget.h regel 50) en wordt er nergens expliciet gedisabled, dus de opbouw-show'en op 154/156/222 leunen niet aantoonbaar op het enable-neveneffect: alleen het toetsenbordvoorbeeld doet dat, en wel op beide kanten. Er zitten in deze vier bestanden geen bestaande workarounds (geen enable/disable vlak na een show/hide, geen commentaar dat de enable-toestand terugzet) — die staan in de bibliotheek zelf. Drie daarvan raken deze voorbeelden rechtstreeks en verdienen aandacht bij dezelfde refactoring: crt_TouchscreenButton.h regels 295-310, waar het commentaar bij enable() letterlijk uitlegt dat Widget::show() enable(false) aanroept en daarmee een lopende druk wiste (die verklaring vervalt, de guard zelf blijft nuttig); crt_TouchscreenButton.h regels 270-283, waar bij disable() staat 'Widget::hide() disabelt ook, dus dit dekt hide() mee' — dat klopt straks niet meer, en het beschreven scenario (knop verbergt zijn eigen pagina, vinger gaat los in het niets) is precies wat tsOutsideKeyboardButton in het toetsenbordvoorbeeld doet; en crt_TouchscreenKeyboard.h regels 263, 295 en 327, waar switchToLower/Upper/Numerical elk 'Base::show(true); // redraw this keyboard' doen — dat is duidelijk redraw(true) bedoeld, en het is nu ook de reden dat een verborgen toetsenbord door een SHIFT- of 123-druk zichtbaar én enabled zou terugspringen. Voor crt_TestAsyncDisplay.h geldt een koppeling tussen regel 143 en 148: die twee zijn een paar, en show->redraw op 143 mag alleen samen met een aanpassing van 148, anders blijft het paneel na de eerste hide non-drawable en verdwijnt het uit beeld. Ik heb aangenomen dat redraw(bIncludeChildren) zichzelf en zijn kinderen tekent zonder enige vlag aan te raken en dat hide() eerst makeNonDrawable doet en daarna repaintRegion (anders zou het toetsenbord zichzelf terugtekenen); die twee volgordes staan in de opdracht maar nog niet in de code, dus ik kon ze niet verifiëren. Ook onzeker: of isDrawable() na de refactoring de publieke lees-API blijft die regel 207 mag gebruiken — als isDrawable() ook verdwijnt, is een eigen bool in TestTouchscreenKeyboard de enige overgebleven optie.

