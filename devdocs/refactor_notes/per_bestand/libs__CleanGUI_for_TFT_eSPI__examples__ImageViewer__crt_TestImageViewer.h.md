# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/ImageViewer/crt_TestImageViewer.h`

8 call sites.

## Per call site

### regel 306 — `button.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** Alleen hertekenen met de zojuist omgewisselde bevelkleuren, zodat de gebruiker ziet dat de tik is aangekomen (commentaar 288-291). Kinderen mee vanwege het label.

**Wordt:** button.redraw(true);  — met show(true) zou een kruisknop die intussen niet-tekenbaar is gemaakt (setFrameButtonsPresent(false)) alsnog tekenbaar worden en op het scherm verschijnen; dat pad is bereikbaar via flagFrameReleased op regel 629 nadat prev/next het plaatje al heeft gewisseld.

### regel 327 — `pButton->setDrawable(bPresent, true);`  *(risico: compileerfout)*

**Bedoeling:** Het kruisje uit de tekening halen bij een plaatje dat geen spritesheet is. Het commentaar op 323-325 wijst er nadrukkelijk op dat de tweede parameter bIncludeChildren is en niet de toestand — de auteur weet hier dus al dat tekenbaar en enabled twee dingen zijn.

**Wordt:** if (bPresent) { pButton->makeDrawable(true); } else { pButton->makeNonDrawable(true); }  — bewust géén show()/hide(), want het commentaar op 319 zegt 'Draws nothing by itself'.

### regel 331 — `pButton->enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Kruisknop weer laten luisteren zodra er een spritesheet op staat.

**Wordt:** ongewijzigd

### regel 335 — `pButton->disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Kruisknop laten zwijgen als hij niet zichtbaar is — commentaar 316-318: 'a button that is no longer visible but still listening would swallow touches meant for whatever is there now'.

**Wordt:** ongewijzigd

### regel 480 — `backgroundPanel.repaintRegion(topLeftPix, sizePix);`  *(risico: mechanisch)*

**Bedoeling:** Alleen de oude plus de nieuwe plaatjesrechthoek door de tree laten hertekenen; commentaar 473-476 legt uit dat daar geen show() bij hoort ('No show() calls needed').

**Wordt:** ongewijzigd — mits redrawRegion() op _bDrawable gaat poorten in plaats van op het verdwijnende _bVisible.

### regel 490 — `backgroundPanel.repaintRegion(crossTopLeftPix, crossSizePix);`  *(risico: mechanisch)*

**Bedoeling:** Tweede, losse regio voor de strook van het kruisje, alleen als dat verschijnt of verdwijnt (commentaar 483-486).

**Wordt:** ongewijzigd

### regel 496 — `captionText.redraw();`  *(risico: mechanisch)*

**Bedoeling:** Het onderschrift ligt buiten beide regio's en verandert elke keer; opaak, dus één pass wist de oude regel en schrijft de nieuwe (commentaar 493-495).

**Wordt:** ongewijzigd — zelfde naambotsing Text::redraw() / Widget::redraw(bool) als in MultiPanel:424.

### regel 565 — `page1.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De enige keer dat het hele scherm terecht wordt geschilderd, want er staat nog niets (commentaar 559-561). Bedoeld als 'teken alles', niet als 'zet alles aan'.

**Wordt:** page1.redraw(true);  — dan blijven zowel de tekenbaarheid als de enable-toestand staan zoals setFrameButtonsPresent() ze op regel 563 heeft gezet.

## Leunt op het neveneffect van show/hide

- **regel 306**: Bestaande workaround die kan verdwijnen. showButton() tekent een INGEDRUKTE knop met show(true); dat roept enable(false) aan op een knop die al enabled is. Daarvoor is in crt_TouchscreenButton.h:295-310 een guard gebouwd ('if (!PBase::isEnabled()) { bPressed = false; }') met het commentaar: 'Dat laatste gebeurde wel, want Widget::show() roept enable(false) aan. Wie de knop ingedrukt tekent, deed dus tegelijk alsof hij nooit was ingedrukt.' Zodra show() niet meer enablet, is die guard niet meer nodig.
- **regel 565**: Onbedoelde, schadelijke afhankelijkheid — de enige aantoonbare show()->enable-interactie in deze vier bestanden. Regel 563 roept setFrameButtonsPresent(false) aan, dat de vier kruisknoppen niet-tekenbaar EN disabled maakt (regel 327/335). Twee regels later enablet page1.show(true) via Widget::show -> this->enable(false), recursief over de hele tree, alle vier weer. arAllImages[0] is imageKleurwiel (1x1, geen spritesheet — zie images/_pal8/_allImages.h), dus dit gebeurt bij elke boot: vier onzichtbare maar levende knoppen die tikken opslokken, precies wat het commentaar op 316-318 wil voorkomen. Onder de nieuwe API enablet show() niet meer en verdwijnt deze latente bug vanzelf; er hoeft dus GEEN extra enable/disable bij, mits regel 565 ook redraw(true) wordt in plaats van show(true) (anders maakt makeDrawable het kruisje alsnog zichtbaar).

