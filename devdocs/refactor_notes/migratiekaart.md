## 1. Per bestand

> Werk per bestand **van onder naar boven** — elke geschrapte regel verschuift de nummers eronder.
> Volgorde van de bestanden: eerst de bibliotheek (1.1), dan de apps (1.2–1.4), dan de voorbeelden (1.5).
> Risicolabels: **C** = compileerfout (de compiler wijst je erop), **S** = stil gedrag (compileert, doet iets anders), **M** = mechanisch (recht-toe-recht-aan).

### 1.1 Bibliotheek — hier ontstaat de nieuwe API (eerst doen)

**`libs/CleanGUI/src/crt_IWidget.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 55-56 | `virtual void show(bool)` / `hide(bool)` + commentaar "show implies enabling" | signatuur blijft; commentaar herschrijven (show = makeDrawable + redraw, hide = makeNonDrawable + repaintRegion, géén enable/disable meer) | M |
| — | (nieuw) | `virtual void redraw(bool bIncludeChildren) = 0;` toevoegen bij het blok "drawing", naast `paint()` en `redrawRegion()` | M |
| 58 | `virtual bool isShown() = 0;` | schrappen | C |
| 59 | `virtual void setDrawable(bool bDrawable, bool bIncludeChildren) = 0;` | vervangen door `virtual void makeDrawable(bool bIncludeChildren) = 0;` en `virtual void makeNonDrawable(bool bIncludeChildren) = 0;` | C |
| 60 | `virtual bool isDrawable() = 0;` | **behouden** — `crt_TestTouchscreenKeyboard.h:207` heeft hem nodig als vervanger van `isShown()` | M |

**`libs/CleanGUI/src/internals/crt_Widget.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 30 | `bool _bVisible;` | schrappen | C |
| 50 | `_bVisible(false), _bEnabled(true), _bDrawable(bDrawable), ...` | `_bVisible(false),` schrappen. **Let op de semantische omslag:** de startwaarde was "tekent niets tot de eerste show()", straks is `_bDrawable(true)` de default en tekent alles vanaf constructie mee | S |
| 309-320 | `show()`: `_bVisible = true; this->enable(false); {children->show(true);}` | `void show(bool b) { makeDrawable(b); redraw(b); }` — **de vlag vóór het tekenen**, anders komt een widget die via hide() non-drawable werd nooit meer terug | S |
| 322-334 | `hide()`: `_bVisible = false; this->disable(false); {children->hide(true);}` | `void hide(bool b) { makeNonDrawable(b); repaintRegion(getGlobPosPix(), _sizePix); }` — makeNonDrawable eerst, anders tekent de widget zichzelf terug | S |
| — | (nieuw, naast `paint()` op 337) | `void redraw(bool b) { if (!_bDrawable) return; paint(); if (b) { for (...) _arChildren[i]->redraw(true); } }` | M |
| 357 | `redrawRegion(): if (!_bVisible) { return; }` | `if (!_bDrawable) { return; }` — zonder deze wijziging tekenen alle `repaintRegion()`-aanroepen niets meer | C |
| 385 | commentaar bij `moveTo`: "hide/show would toggle _bVisible en _bEnabled" | herschrijven: hide/show raakt straks `_bDrawable`, niet `_bEnabled` | M |
| 394 | `moveTo(): if (!_bVisible \|\| (_display == nullptr))` | `if (!_bDrawable \|\| (_display == nullptr))` — anders beweegt niets meer | C |
| 518-521 | `bool isShown() { return _bVisible; }` | schrappen | C |
| 538-547 | `void setDrawable(bool bDrawable, bool bIncludeChildren)` | splitsen in `makeDrawable(bool bIncludeChildren)` en `makeNonDrawable(bool bIncludeChildren)` (beide delegerend naar een private `setDrawableImpl`) | C |

**`libs/CleanGUI/src/crt_Panel.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 209-213 | `void show(bool)` override: `showPanel(); WBase::show(...)` | **override schrappen.** `Widget::show` → `redraw()` → `paint()` → `showPanel()` doet het al, en in de juiste volgorde (vlag eerst) | S |
| 215-231 | `void hide(bool)` override met `if (WBase::isShown()) hidePanel();` | **override schrappen** | C |
| 317-330 | `hidePanel()` (vult de eigen rect met `_colBg`) | wordt onbereikbaar → schrappen. Wissen loopt voortaan via `repaintRegion` door de root, wat herstelt wat er hoort te staan in plaats van colBg te smeren | M |
| 285 | `showPanel(): if (!WBase::isDrawable()) return;` | ongewijzigd (dubbele poort, onschadelijk) | M |

**`libs/CleanGUI/src/crt_Text.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 100-103 | `void redraw() { showText(); }` | **naambotsing met `Widget::redraw(bool)`.** Kies één: (a) `Text::redraw()` schrappen en de twee aanroepers op `redraw(false)` zetten, of (b) `Widget::redraw(bool bIncludeChildren = false)` met default. Zonder keuze verbergt `Text::redraw()` de nieuwe overload en faalt `text.redraw(true)` | C |
| 163-166 | commentaar "redrawRegion() knipt een widget weg zodra zijn rect de regio niet overlapt" | blijft waar; alleen de zin over onzichtbaarheid controleren | M |
| 185-189 | `void show(bool)` override | schrappen (zie Panel) | S |
| 194-201 | `void hide(bool)` override met `if (WBase::isShown()) hideText();` | schrappen | C |
| — | `hideText()` | na het schrappen van `hide()` mogelijk ongebruikt — controleren en dan weghalen | M |

**`libs/CleanGUI/src/crt_TouchscreenButton.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 270-283 | `disable()` + commentaar "Widget::hide() disabelt ook, dus dit dekt hide() mee" | code ongewijzigd; **commentaar is straks onwaar** en moet weg. Het beschreven scenario (knop zet zijn eigen pagina weg, vinger gaat los in het niets) wordt nu gedekt door de expliciete `disable()` bij de aanroeper | S |
| 295-310 | `enable()` + guard `if (!PBase::isEnabled()) { bPressed = false; }` | guard **laten staan** (blijft juist voor een echte uit→aan-overgang); het commentaar dat `Widget::show()` enable(false) aanroept vervalt | M |

**`libs/CleanGUI/src/crt_TouchscreenKeyboard.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 263 | `Base::show(true); // redraw this keyboard.` | `Base::redraw(true);` | S |
| 295 | idem (switchToUpper) | `Base::redraw(true);` | S |
| 327 | idem (switchToNumerical) | `Base::redraw(true);` | S |

Zonder deze drie zet een SHIFT- of 123-druk een bewust niet-tekenbaar toetsenbord weer zichtbaar én tekenbaar terug — precies wat `pdga_GameOverDisplayControl.h` in de hall-of-fame-fase uitzet.

---

### 1.2 `apps/pong_cleangui_dg`

**`apps/pong_cleangui_dg/pdg_GameDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 296, 311, 315-320 | commentaar | 315-320 herschrijven: "redrawRegion() breekt af op een onzichtbare widget" → "op een niet-tekenbare widget" | M |
| 324 | `page.setDrawable(true, true);` | vervalt (`show(true)` doet de makeDrawable), of `page.makeDrawable(true);` | C |
| 325 | `page.show(true);` | ongewijzigd | M |
| 326 | `page.enable(true);` | **ongewijzigd — niet opruimen.** Wordt de enige enable van de peddelknoppen | M |
| 329-341 | commentaar "setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat" | herschrijven: `makeNonDrawable(true)` ís nu het hele verbergen | M |
| 344 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 345 | `page.hide(true);` | **schrappen.** De nieuwe hide() repaint de eigen rect en dat is precies wat hier nooit mag (drie tekentaken delen één commandobuffer) | S |
| 346 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 410 | `paddlePanel.show(true);` | ongewijzigd | M |
| 411 | `paddlePanel.enable(true);` | ongewijzigd | M |
| 412 | `buttonLeft.show(true);` | ongewijzigd | M |
| 413 | `buttonLeft.enable(true);` | **ongewijzigd — niet opruimen** (enige enable) | M |
| 414 | `buttonRight.show(true);` | ongewijzigd | M |
| 415 | `buttonRight.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 419 | `paddlePanel.hide(true);` | ongewijzigd (moet hier wél wissen; loopt nu via repaintRegion, doellijn komt terug i.p.v. weggevuld) | M |
| 420 | `paddlePanel.disable(true);` | ongewijzigd | M |
| 421 | `buttonLeft.hide(true);` | ongewijzigd | M |
| 422 | `buttonLeft.disable(true);` | **ongewijzigd — niet opruimen** (enige route naar `TouchscreenButton::disable()`, die bPressed reset) | M |
| 423 | `buttonRight.hide(true);` | ongewijzigd | M |
| 424 | `buttonRight.disable(true);` | **ongewijzigd — niet opruimen** | M |

**`apps/pong_cleangui_dg/pdg_StartPageDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 112-113 | commentaar (verwijst naar GameDisplayControl::showPage) | meewijzigen | M |
| 116 | `page.setDrawable(true, true);` | vervalt, of `page.makeDrawable(true);` | C |
| 117 | `page.show(true);` | ongewijzigd | M |
| 118 | `page.enable(true);` | **ongewijzigd — niet opruimen.** Enige regel die "Click here to Start" laat werken | M |
| 123 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 124 | `page.hide(true);` | **schrappen** | S |
| 125 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |

**`apps/pong_cleangui_dg/pdg_GameOverDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 124-125 | commentaar | meewijzigen | M |
| 128 | `page.setDrawable(true, true);` | vervalt, of `page.makeDrawable(true);` | C |
| 129 | `page.show(true);` | ongewijzigd | M |
| 130 | `page.enable(true);` | **ongewijzigd — niet opruimen.** Anders is er geen weg meer uit de game-over-pagina | M |
| 135 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 136 | `page.hide(true);` | **schrappen** | S |
| 137 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |

`pdg_GameControl.h`: geen widget-aanroepen. `paddleControl.enable()/disable()` (L53/L62) zijn control-objecten — **niet aanpassen**.

---

### 1.3 `apps/pong_cleangui_cg`

**`apps/pong_cleangui_cg/pcg_DisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 434, 437-438 | commentaar (somt `setDrawable` op) | herschrijven naar makeNonDrawable | M |
| 446 | `page.setDrawable(true, true);` | vervalt, of `page.makeDrawable(true);` | C |
| 447 | `page.show(true);` | ongewijzigd | M |
| 448 | `page.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 453 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 454 | `page.hide(true);` | **schrappen** — 453 ís al wat hier bedoeld werd | S |
| 455 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 473-481 | commentaarblok over de `_bVisible`-poort van redrawRegion | herschrijven; de reden die het geeft bestaat niet meer | M |
| 482 | `pageRoot.show(false);` | `pageRoot.makeDrawable(false);` of **schrappen** (`_bDrawable` staat bij constructie al true) | M |
| 567 | `paddlePanel.show(true);` | ongewijzigd | M |
| 568 | `paddlePanel.enable(true);` | ongewijzigd (puur administratief) | M |
| 569 | `buttonLeft.show(true);` | ongewijzigd | M |
| 570 | `buttonLeft.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 571 | `buttonRight.show(true);` | ongewijzigd | M |
| 572 | `buttonRight.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 576 | `paddlePanel.hide(true);` | ongewijzigd | M |
| 577 | `paddlePanel.disable(true);` | ongewijzigd (administratief) | M |
| 578 | `buttonLeft.hide(true);` | ongewijzigd | M |
| 579 | `buttonLeft.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 580 | `buttonRight.hide(true);` | ongewijzigd | M |
| 581 | `buttonRight.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 594-595 | commentaar "onzichtbare widget" | → "niet-tekenbare widget" | M |

`pcg_GameControl.h`: geen widget-aanroepen (L49/L58 zijn `PaddleControl`). **Niet aanpassen.**

---

### 1.4 `apps/pong_cleangui_dg_adv` — ONTBREEKT IN DE RUWE DATA

Deze drie bestanden staan niet in de aangeleverde inventarisatie maar bevatten samen **54 call sites**, waaronder de gevaarlijkste van de hele migratie: negen page-brede `show(true)`-aanroepen die dienstdoen als frame-redraw terwijl er in diezelfde boom bewust widgets niet-tekenbaar staan. Zelf geïnventariseerd.

**`apps/pong_cleangui_dg_adv/pdga_GameDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 558 | commentaar "een positie en setDrawable(true)" | → `makeDrawable(true)` | M |
| 571 | `panel.setDrawable(false, true);` (ballen, opbouw) | `panel.makeNonDrawable(true);` | C |
| 585 | `panel.setDrawable(false, true);` (crates) | `panel.makeNonDrawable(true);` | C |
| 598 | `panel.setDrawable(false, true);` (vuurkorven) | `panel.makeNonDrawable(true);` | C |
| 684 | `buttonGameOverLabel.setDrawable(false, true);` | `.makeNonDrawable(true);` | C |
| 742 | commentaar "redrawRegion() breekt af op een onzichtbare widget" | herschrijven | M |
| 750 | `page.setDrawable(true, true);` | `page.makeDrawable(true);` — **moet blijven staan**, mag hier niet vervallen | C |
| 752-754 | `buttonGameOverLabel.setDrawable(false, true);` | `.makeNonDrawable(true);` | C |
| 755 | `buttonGameOverLabel.disable(true);` | ongewijzigd | M |
| 757 | `page.show(true);` | **`page.redraw(true);`** — met `show(true)` maakt de makeDrawable regel 754 ongedaan en verschijnt het GAME OVER-vignet bij elk nieuw spel | **S (kritiek)** |
| 758 | `page.enable(true);` | ongewijzigd; **let op:** dit enablet ook `buttonGameOverLabel` weer, dus 755 moet ná 758 of de disable moet herhaald | S |
| 761 | commentaar "setDrawable(false) eerst" | herschrijven | M |
| 776 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 777 | `page.hide(true);` | **schrappen** | S |
| 778 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 846, 869, 901 | `panel.setDrawable(false, true);` | `panel.makeNonDrawable(true);` | C |
| 850, 873, 905 | `panel.setDrawable(true, true);` | `panel.makeDrawable(true);` | C |
| 1025-1027 | `paddlePanel/buttonLeft/buttonRight.setDrawable(true, true);` | `.makeDrawable(true);` (of laten vervallen: de `show(true)` op 1029-1033 doet het) | C |
| 1029, 1031, 1033 | `.show(true);` | ongewijzigd | M |
| 1030, 1032, 1034 | `.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 1040 | commentaar "setDrawable(false) en niet hide()" | → makeNonDrawable | M |
| 1045-1047 | `.setDrawable(false, true);` (buffered pad) | `.makeNonDrawable(true);` | C |
| 1051-1053 | `.hide(true);` (onbuffered pad) | ongewijzigd | M |
| 1056-1058 | `.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 1064 | `buttonGameOverLabel.setDrawable(true, true);` | `.makeDrawable(true);` | C |
| 1065 | `.enable(true);` | ongewijzigd | M |
| 1066 | `pageGame.show(true);` | **`pageGame.redraw(true);`** — anders komen alle 16 balpanelen, alle crates en de zojuist verborgen peddel weer tekenbaar in beeld | **S (kritiek)** |
| 1070 | `buttonGameOverLabel.setDrawable(false, true);` | `.makeNonDrawable(true);` | C |
| 1071 | `.disable(true);` | ongewijzigd | M |
| 1145 | `pageGame.show(true);` (frame-redraw, buffered) | **`pageGame.redraw(true);`** — dit is de heetste regel van de repo: hij draait elk beeld | **S (kritiek)** |

**`apps/pong_cleangui_dg_adv/pdga_GameOverDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 34 | commentaar "Wisselen gaat met setDrawable()" | → makeDrawable/makeNonDrawable | M |
| 350-353 | commentaar "setDrawable en niet hide()" | herschrijven | M |
| 362 | `hallOfFameTitlePanel.setDrawable(!bName, true);` | via helper (zie patroon P5) | C |
| 364-366 | `typeYourNameText/inputfield/touchscreenKeyboard.setDrawable(bName, true);` | via helper | C |
| 370-371 | `arHallOfFameNames[i]/arHallOfFameScores[i].setDrawable(!bName, true);` | via helper | C |
| 373-375 | `buttonStart/arTorchPanels[0..1].setDrawable(!bName, true);` | via helper | C |
| 389-391 | `inputfield.enable(bName);` / `touchscreenKeyboard.enable(bName && bKeyboardIsUp);` / `buttonStart.enable(!bName);` | **ongewijzigd, maar wordt dragend**: vandaag worden deze drie een regel later door `page.show(true)` weer allemaal op `true` gezet | S |
| 461 | `touchscreenKeyboard.disable(true);` | ongewijzigd | M |
| 483 | `page.show(true);` (per schuifstap) | **`page.redraw(true);`** | **S (kritiek)** |
| 497 | `touchscreenKeyboard.enable(true);` | ongewijzigd | M |
| 505 | `page.setDrawable(true, true);` | `page.makeDrawable(true);` — **moet blijven** | C |
| 506 | `drawAndEnableWidgetsForPhase(phase);` | ongewijzigd | M |
| 507 | `page.show(true);` | **`page.redraw(true);`** — met `show(true)` staan toetsenbord én hall-of-fame-lijst tegelijk op het scherm | **S (kritiek)** |
| 508 | `page.enable(true);` | **verplaatsen naar vóór regel 506**, anders overschrijft hij de fase-enables van 389-391 (dat gebeurt vandaag ook, maar wordt na de refactoring de enige plek waar het nog misgaat) | S |
| 513 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 514 | `page.hide(true);` | **schrappen** | S |
| 515 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 573 | `pageGameOver.show(true);` (fakkeltik) | **`pageGameOver.redraw(true);`** | **S (kritiek)** |
| 609 | `pageGameOver.show(true);` (na PHASE_NAME_INPUT) | **`pageGameOver.redraw(true);`** | **S (kritiek)** |
| 625 | `pageGameOver.show(true);` (na PHASE_HALLOFFAME) | **`pageGameOver.redraw(true);`** | **S (kritiek)** |
| 639 | `pageGameOver.show(true);` (nieuwe toets) | **`pageGameOver.redraw(true);`** | **S (kritiek)** |

**`apps/pong_cleangui_dg_adv/pdga_StartPageDisplayControl.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 179 | `page.setDrawable(true, true);` | vervalt, of `page.makeDrawable(true);` | C |
| 180 | `page.show(true);` | ongewijzigd (niets in deze pagina staat bewust niet-tekenbaar) | M |
| 181 | `page.enable(true);` | **ongewijzigd — niet opruimen** | M |
| 186 | `page.setDrawable(false, true);` | `page.makeNonDrawable(true);` | C |
| 187 | `page.hide(true);` | **schrappen** | S |
| 188 | `page.disable(true);` | **ongewijzigd — niet opruimen** | M |
| 251 | `pageStart.show(true);` (fakkeltik) | `pageStart.redraw(true);` | S |

---

### 1.5 `libs/CleanGUI_for_TFT_eSPI/examples`

**`MultiPanel/crt_TestMultiPanel.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 265 | `button.show(true);` | `button.redraw(true);` | M |
| 334 | `panel.setDrawable(true, true);` | `panel.makeDrawable(true);` (334+335 samen mag ook `panel.show(true);`) | C |
| 335 | `panel.show(false);` | `panel.redraw(false);` | M |
| 346 | `arSpritePanels[i].setDrawable(false, true);` | `.makeNonDrawable(true);` — **bewust géén hide()**, dat zou de veertig losse wissen terugbrengen die 352-354 juist vermijdt | C |
| 352-354, 458-462 | commentaar over setDrawable | herschrijven | M |
| 355 | `mapPanel.repaintRegion(...)` | ongewijzigd — werkt alleen als `redrawRegion()` op `_bDrawable` poort | M |
| 424 | `fpsText.redraw();` | ongewijzigd; zie de naambotsing bij `crt_Text.h:100` | M |
| 471 | `panel.setDrawable(false, true);` | `.makeNonDrawable(true);` | C |
| 490 | `page1.show(true);` | **`page1.redraw(true);`** — `show(true)` zet de 40 spritepanelen van 471 tekenbaar en zet ze alle veertig op (0,0) | **S** |
| 513 | `_pPage->show(true);` (elk beeld) | **`_pPage->redraw(true);`** — zelfde reden, elk beeld opnieuw | **S** |

**`Panel/crt_TestPanel.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 149 | `panel.show(false);` | `panel.redraw(false);` | M |
| 151 | `panelImageCentered.show(false);` | `.redraw(false);` | M |
| 153 | `panelImageClipped.show(false);` | `.redraw(false);` | M |
| 155 | `panelBackdrop.show(false);` | `.redraw(false);` | M |
| 157 | `panelImageTransparent.show(false);` | `.redraw(false);` | M |

**`Inputfield/crt_TestInputfield.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 152-154 | commentaar "_bVisible en _bEnabled blijven onaangeroerd" | → `_bDrawable` en `_bEnabled` | M |
| 169 | `touchscreenKeyboard.disable(true);` | ongewijzigd | M |
| 205 | `touchscreenKeyboard.enable(true);` | ongewijzigd | M |
| 252 | `page1.show(true);` | **`page1.redraw(true);`** — `show(true)` maakt `tsOutsideInputfieldButton` tekenbaar, die in `crt_InputField.h:70` met `/*drawable*/false` wordt gebouwd | **S** |
| 308 | `inputfield.enable(true);` | ongewijzigd | M |

**`ImageViewer/crt_TestImageViewer.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 306 | `button.show(true);` | `button.redraw(true);` — met `show(true)` wordt een kruisknop die via `setFrameButtonsPresent(false)` niet-tekenbaar is alsnog zichtbaar (bereikbaar via `flagFrameReleased`, regel 629) | **S** |
| 316-325 | commentaar | meewijzigen | M |
| 327 | `pButton->setDrawable(bPresent, true);` | `if (bPresent) { pButton->makeDrawable(true); } else { pButton->makeNonDrawable(true); }` — bewust géén show()/hide() | C |
| 331 | `pButton->enable(true);` | ongewijzigd | M |
| 335 | `pButton->disable(true);` | ongewijzigd | M |
| 480 | `backgroundPanel.repaintRegion(topLeftPix, sizePix);` | ongewijzigd (hangt af van de `_bDrawable`-poort in `redrawRegion`) | M |
| 490 | `backgroundPanel.repaintRegion(crossTopLeftPix, crossSizePix);` | ongewijzigd | M |
| 496 | `captionText.redraw();` | ongewijzigd; naambotsing, zie `crt_Text.h:100` | M |
| 565 | `page1.show(true);` | **`page1.redraw(true);`** — behoudt zowel de tekenbaarheid als de enable-toestand die 563 net heeft gezet | **S** |

**`TouchscreenKeyboard/crt_TestTouchscreenKeyboard.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 156 | `page1.show(true);` | ongewijzigd (`show(true)` is hier juist gewenst: het toetsenbord moet ook tekenbaar zijn) | M |
| 180 | `touchscreenKeyboard.hide(true);` | `hide(true);` **+ nieuwe regel** `touchscreenKeyboard.disable(true);` | **S — leunt op neveneffect** |
| 207 | `if(!touchscreenKeyboard.isShown())` | `if(!touchscreenKeyboard.isDrawable())` — let op de andere startwaarde (true vanaf constructie i.p.v. false), hier onschuldig omdat dit punt pas na `page1.show(true)` bereikt wordt | C |
| 209 | `touchscreenKeyboard.show(true);` | `show(true);` **+ nieuwe regel** `touchscreenKeyboard.enable(true);` (enable ná show) | **S — leunt op neveneffect** |

**`AsyncDisplay/crt_TestAsyncDisplay.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 143 | `page1.show(true);` | ongewijzigd laten — **gekoppeld aan 148**: `redraw(true)` mag hier alleen als 148 óók verandert, anders blijft het paneel na de eerste hide voorgoed niet-tekenbaar | S |
| 148 | `panel.hide(true);` | ongewijzigd; netter is 148+149 te vervangen door `panel.moveTo(locPos, CoordType::Pixels)` (en dan mag 143 wél `redraw(true)` worden) | M |

**`TouchscreenButtonGroup/crt_TestTouchscreenButtonGroup.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 222 | `page1.show(true);` | ongewijzigd; geen enable nodig (niets is ooit gedisabled) | M |

**`TouchscreenButton/crt_TestTouchscreenButton.h`**

| Regel | Huidige aanroep | Nieuwe aanroep | Risico |
|---|---|---|---|
| 154 | `page1.show(true);` | ongewijzigd; geen enable nodig | M |

**Nul wijzigingen** (geverifieerd: geen enkele `show(`, `hide(`, `setDrawable(`, `isShown(`, `redraw(`, `repaintRegion(`): `apps/keukenweegschaal/weegcontrol.h` (L55, L78), `apps/keukenweegschaal/weegcontrol_obs.h` (L20, L71, L113), `apps/pong/pong/crt_GameControl.h` (L127, L138). De `enable()`/`disable()` daar staan op een pseudocode-`display`-object en op een CleanRTOS-Task achter `ISceneDisplayControl`, niet op een widget. Een mechanische zoek-vervang op `enable(` mag deze bestanden niet raken.

---

## 2. LEUNT OP NEVENEFFECT — hier moet expliciet enable()/disable() bij

### 2a. Echte gaten: zonder extra regel gaat het stuk

| Bestand | Regel | Wat er verdwijnt | Wat erbij moet |
|---|---|---|---|
| `libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenKeyboard/crt_TestTouchscreenKeyboard.h` | 180 | `hide(true)` was de **enige** plek die het toetsenbord uitzet; er staat nergens `disable()`. Touchafhandeling kijkt alleen naar `_bEnabled` (`TouchscreenButton::touchPressed` L219, `TouchscreenButtonGroup::touchPressed` L77) | `touchscreenKeyboard.disable(true);` **ná** de hide. Zonder dit blijft het onzichtbare toetsenbord toetsen versturen, neemt de if/else-keten 173-211 altijd de `queueKeyPressedStrings`-tak, draait de `flagTouchPressed`-tak op 203 nooit meer en komt het toetsenbord nooit meer terug — en `tsOutsideKeyboardButton` blijft bij elke aanraking "OutsideKeyboardPressed" vuren |
| `libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenKeyboard/crt_TestTouchscreenKeyboard.h` | 209 | `show(true)` was de **enige** plek die het toetsenbord na de hide van 180 weer aanzet | `touchscreenKeyboard.enable(true);` **ná** de show (niet ervoor: geen aanraking op een half getekend toetsenbord). Zonder dit komt het toetsenbord wel in beeld maar blijft het dood — nooit meer een `keyPressed`, dus de tekstuitvoer op 194 werkt na de eerste hide niet meer |

### 2b. Regels die vandaag redundant zijn en na de refactoring **dragend** worden — NIET opruimen

Dit is het spiegelbeeld en in de praktijk het grotere risico: elke regel hieronder ziet er nu uit als dubbelop en is straks de enige enable/disable. Wie ze bij een opschoonronde schrapt, krijgt een dode startknop, een pagina die onzichtbaar blijft reageren, of een knop met een blijvend hangende `bPressed`.

- `apps/pong_cleangui_dg/pdg_GameDisplayControl.h`: **326**, **346**, **411**, **413**, **415**, **420**, **422**, **424**
- `apps/pong_cleangui_dg/pdg_StartPageDisplayControl.h`: **118**, **125**
- `apps/pong_cleangui_dg/pdg_GameOverDisplayControl.h`: **130**, **137**
- `apps/pong_cleangui_cg/pcg_DisplayControl.h`: **448**, **455**, **570**, **572**, **579**, **581** (functioneel) — 568 en 577 zijn puur administratief (alleen `TouchscreenButton` leest `isEnabled()`), die mogen blijven staan of weg
- `apps/pong_cleangui_dg_adv/pdga_GameDisplayControl.h`: **755**, **758**, **778**, **1030**, **1032**, **1034**, **1056**, **1057**, **1058**, **1065**, **1071**
- `apps/pong_cleangui_dg_adv/pdga_GameOverDisplayControl.h`: **389**, **390**, **391**, **508**, **515**
- `apps/pong_cleangui_dg_adv/pdga_StartPageDisplayControl.h`: **181**, **188**
- `libs/CleanGUI_for_TFT_eSPI/examples/ImageViewer/crt_TestImageViewer.h`: **331**, **335**
- `libs/CleanGUI_for_TFT_eSPI/examples/Inputfield/crt_TestInputfield.h`: **169**, **205**, **308**

Twee daarvan zijn extra scherp omdat `TouchscreenButton::disable()` de `bPressed`-reset draagt: `pdg_GameDisplayControl.h:422/424` en `pcg_DisplayControl.h:579/581`. Zonder die regels blijft een knop die onder je vinger verdween voorgoed "ingedrukt" en meldt hij nooit meer een `buttonPressed()`.

### 2c. Latente bugs die door de refactoring vanzelf verdwijnen (géén compensatie toevoegen)

| Bestand | Regel | Wat er nu misgaat |
|---|---|---|
| `libs/.../ImageViewer/crt_TestImageViewer.h` | 565 | 563 roept `setFrameButtonsPresent(false)` aan (kruisknoppen niet-tekenbaar **en** disabled), waarna `page1.show(true)` ze recursief alle vier weer enablet. `arAllImages[0]` is `imageKleurwiel` (geen spritesheet), dus dit gebeurt bij **elke boot**: vier onzichtbare maar levende knoppen die tikken opslokken. Verdwijnt mits 565 ook `redraw(true)` wordt |
| `apps/pong_cleangui_dg_adv/pdga_GameDisplayControl.h` | 1145 | Het frame-brede `pageGame.show(true)` enablet elk beeld de hele boom — inclusief `buttonLeft`/`buttonRight` die bij `flagHidePaddle` (1056-1058) net gedisabled zijn en `buttonGameOverLabel` (1071). De peddelknoppen blijven dus na game-over touch-actief. Verdwijnt met `redraw(true)` |
| `apps/pong_cleangui_dg_adv/pdga_GameOverDisplayControl.h` | 573, 609, 625, 639 | Elk van deze `show(true)`-aanroepen enablet de hele pagina en overschrijft daarmee de fase-enables uit 389-391: in de naamfase wordt `buttonStart` weer aanraakbaar en het toetsenbord ook als het nog niet omhoog staat. Verdwijnt met `redraw(true)` |
| `libs/.../MultiPanel/crt_TestMultiPanel.h` | 265, 513 | `showButton()` tekent een ingedrukte knop met `show(true)`, en 513 herhaalt de enable van de hele boom elk beeld terwijl er een vinger op een knop ligt. Alleen de guard in `TouchscreenButton::enable()` voorkomt nu dat `bPressed` daarbij sneuvelt. De knoppen worden hier nooit gedisabled, dus er hoeft **geen** enable bij |

---

## 3. WORKAROUNDS DIE WEG KUNNEN

| Bestand | Regel | Wat het is | Waarom het weg kan |
|---|---|---|---|
| `libs/CleanGUI/src/crt_Panel.h` | 215-231 | `hide()`-override met `if (WBase::isShown()) hidePanel();` en het commentaarblok over de peddelwis die over de startknop viel | De nieuwe `hide()` herstelt via `repaintRegion` wat er hoort te staan in plaats van `colBg` te smeren; de vraag "stond er ooit iets?" is dan zinloos |
| `libs/CleanGUI/src/crt_Panel.h` | 317-330 | `hidePanel()` (fillRect met `_colBg`) | Wordt onbereikbaar |
| `libs/CleanGUI/src/crt_Panel.h` | 209-213 | `show()`-override (`showPanel()` vóór het vlagzetten) | `Widget::show` → `redraw` → `paint` doet het, en de volgordeval (tekenen vóór `makeDrawable`) verdwijnt daarmee |
| `libs/CleanGUI/src/crt_Text.h` | 185-201 | `show()`- en `hide()`-override met dezelfde `isShown()`-truc | Idem |
| `libs/CleanGUI/src/crt_TouchscreenButton.h` | 295-310 | Het **commentaar** bij de guard: "Dat laatste gebeurde wel, want Widget::show() roept enable(false) aan" | `show()` enablet niet meer; de guard zelf blijft nuttig, de verklaring niet |
| `libs/CleanGUI/src/crt_TouchscreenButton.h` | 278 | Commentaar "Widget::hide() disabelt ook, dus dit dekt hide() mee" | Wordt onwaar en is misleidend voor de volgende lezer |
| `apps/pong_cleangui_cg/pcg_DisplayControl.h` | 446 | `setDrawable(true,true)` vlak vóór `show(true)` | `show()` doet de makeDrawable zelf |
| `apps/pong_cleangui_cg/pcg_DisplayControl.h` | 454 | `hide(true)` ná `setDrawable(false,true)` | 453 ís al wat hier bedoeld werd |
| `apps/pong_cleangui_cg/pcg_DisplayControl.h` | 473-482 | `pageRoot.show(false)` + het commentaarblok dat uitlegt waarom `redrawRegion()` anders afbreekt | `_bDrawable` staat bij constructie al true; de poort waar het commentaar over gaat bestaat straks niet meer |
| `apps/pong_cleangui_dg/pdg_*DisplayControl.h` | 324, 345 / 116, 124 / 128, 136 | Zelfde `setDrawable`-vóór-`show` en `hide`-ná-`setDrawable(false)` | Idem |
| `apps/pong_cleangui_dg_adv/pdga_*.h` | 777, 514, 187 | `page.hide(true)` in hidePage | Idem — hier extra dringend: de repaint uit de nieuwe `hide()` is precies wat de drie tekentaken elkaar niet mogen aandoen |
| Alle `hidePage()`-commentaarblokken | `pdg_GameDisplayControl.h:329-341`, `pcg_DisplayControl.h:435-443`, `pdga_GameDisplayControl.h:761-773` | "setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak overslaat" | Dat ís nu de definitie van `makeNonDrawable`; de constructie waar het commentaar voor waarschuwt bestaat niet meer |
| `libs/.../AsyncDisplay/crt_TestAsyncDisplay.h` | 148-149 | `panel.hide(true); setLocPos(...)` | `moveTo()` bestaat precies hiervoor en flikkert niet |
| `libs/.../Inputfield/crt_TestInputfield.h` | 152-154 | Commentaar dat uitlegt dat `moveTo` `_bVisible`/`_bEnabled` met rust laat | `_bVisible` bestaat niet meer |

---

## 4. Patronen en hun standaardvertaling

**P1 — Pagina tonen** (`pdg` ×3, `pcg`, `pdga` ×3)
```cpp
page.setDrawable(true, true);   page.show(true);   page.enable(true);
```
→ **Als er in die subboom niets bewust niet-tekenbaar is** (`pdg`, `pcg`, `pdga_StartPage`):
```cpp
page.show(true);                // == makeDrawable + redraw
page.enable(true);              // dragend geworden - niet opruimen
```
→ **Als er wél widgets bewust niet-tekenbaar staan** (`pdga_GameDisplayControl:750-758`, `pdga_GameOverDisplayControl:505-508`):
```cpp
page.makeDrawable(true);
page.enable(true);              // vóór de fase-instelling, anders overschrijft hij die
<fase-/vignetinstelling: makeNonDrawable + disable van wat níet mee mag>
page.redraw(true);              // NOOIT show(true) hier
```

**P2 — Pagina verbergen zonder te tekenen** (`pdg` ×3, `pcg`, `pdga` ×3)
```cpp
page.setDrawable(false, true);   page.hide(true);   page.disable(true);
```
→
```cpp
page.makeNonDrawable(true);     // dit ís het hele verbergen
page.disable(true);             // dragend geworden - niet opruimen
```
De `hide()` **moet** weg: hij zet nu een `repaintRegion` in een commandobuffer die door drie tekentaken wordt gedeeld.

**P3 — Page-brede `show(true)` als frame-redraw** (`MultiPanel:490/513`, `ImageViewer:565`, `Inputfield:252`, `pdga_GameDisplayControl:1066/1145`, `pdga_GameOverDisplayControl:483/573/609/625/639`, `pdga_StartPageDisplayControl:251`)
→ **altijd `redraw(true)`**, nooit `show(true)`. Dit is de talrijkste stille valstrik: `show()` raakte `_bDrawable` vroeger nooit aan, en doet dat straks wel — elke page-brede `show(true)` maakt daarmee opzettelijk uitgezette widgets weer tekenbaar.

**P4 — Een knop hertekenen in ingedrukte kleuren** (`MultiPanel:265`, `ImageViewer:306`)
```cpp
button.show(true);   →   button.redraw(true);
```

**P5 — `setDrawable()` met een variabele** (`ImageViewer:327`, `pdga_GameOverDisplayControl:362-375`, 11 plekken)
```cpp
w.setDrawable(bCond, true);
```
→ per plek uitschrijven:
```cpp
if (bCond) { w.makeDrawable(true); } else { w.makeNonDrawable(true); }
```
of, voor `pdga_GameOverDisplayControl` met zijn elf gevallen, één lokale helper in dat bestand:
```cpp
static void setDrawable(IWidget& w, bool bDrawable)
{ if (bDrawable) { w.makeDrawable(true); } else { w.makeNonDrawable(true); } }
```

**P6 — `setDrawable(false, true)` in de opbouw** (`MultiPanel:346/471`, `pdga_GameDisplayControl:571/585/598/684/846/869/901/1045-1047`, `pdga:1025-1027/850/873/905` voor de aan-kant)
→ letterlijk `makeNonDrawable(true)` / `makeDrawable(true)`. Bewust **géén** `hide()`/`show()`: `hide()` zou per widget een repaint insturen — precies de "veertig losse wissen" die `MultiPanel:352-354` vermijdt.

**P7 — `hide()` dat wél moet wissen** (`pdg:419/421/423`, `pcg:576/578/580`, `pdga:1051-1053`, `AsyncDisplay:148`)
→ blijft `hide(true)`. Gedragsverschil: het wissen loopt nu via `repaintRegion` door de root in plaats van via een `_colBg`-vulling. Waar de achtergrond zwart is, is het resultaat gelijk; op de spelpagina is het netter, want de doellijn komt terug in plaats van weggevuld te worden.

**P8 — De poort `if (!_bVisible) return;`** (`crt_Widget.h:357` in `redrawRegion`, `:394` in `moveTo`)
→ `if (!_bDrawable) return;`. Zonder deze twee tekenen `MultiPanel:355`, `ImageViewer:480/490` en `pdg:435/441` niets meer en beweegt er niets meer.

**P9 — Naamgelijkenis die géén widget is** (`pcg_GameControl.h:49/58`, `pdg_GameControl.h:53/62`, `apps/keukenweegschaal/*`, `apps/pong/pong/crt_GameControl.h:127/138`)
→ **niet aanraken.** Een mechanische zoek-vervang op `enable(`/`disable(` moet deze zeven plekken overslaan.

---

## 5. Open vragen / niet-eenduidige vertalingen

1. **`Text::redraw()` botst met `Widget::redraw(bool)`.** `crt_Text.h:100` heeft al een `redraw()` zonder argumenten; die verbergt de nieuwe overload. `MultiPanel:424` en `ImageViewer:496` blijven compileren, maar `someText.redraw(true)` niet. Kies vóór het schrijven van de bibliotheek: `Text::redraw()` schrappen, of `Widget::redraw(bool bIncludeChildren = false)` een default geven. Die keuze bepaalt of P3 als `redraw(true)` of als `redraw()` geschreven wordt.

2. **Cullt `redraw()`/`redrawRegion()` de subboom op `_bDrawable`?** Vandaag cullt `redrawRegion` op `_bVisible` en slaat het tekenen pas in `showPanel()`/`showText()` over op `_bDrawable` — maar hij **loopt wel door naar de kinderen**. Een `_bDrawable`-poort bovenaan cullt de kinderen mee. In de huidige repo maakt dat niets uit, want élke `setDrawable()` gebeurt met `bIncludeChildren = true` (geverifieerd, alle 30+ plekken). Leg de keuze wel expliciet vast, want `hide(bIncludeChildren = false)` op een ouder met nog-tekenbare kinderen laat die kinderen anders staan (`pcg:578/580` en `pdg:421/423` zijn precies dat geval: een `TouchscreenButton` met een `Text`-kind).

3. **Via welke node repaint `hide()`?** "makeNonDrawable + repaintRegion(eigen rect)" werkt alleen goed als de repaint via de **root** loopt (dan komt terug wat er hoort te staan). Loopt hij via de widget zelf, dan tekent een niet-tekenbare widget niets en blijft er een gat. `Widget::moveTo` gebruikt al `repaintRegion` op zichzelf; controleer of die intern naar de root doorschakelt.

4. **De startwaarde is omgedraaid.** Was: `_bVisible = false` — er tekent niets tot de eerste `show()`. Wordt: `_bDrawable = true` — alles tekent mee vanaf constructie. Elke pagina moet dus in de opbouwfase expliciet niet-tekenbaar gemaakt worden. `pcg` doet dat al (495/503/509), `pdg` en `pdga` ook via hun eerste `hidePage()`, maar **in het venster tussen `addChildWidget()` en die eerste `hidePage()` is een subboom straks wél tekenbaar** — en in `pdga` draaien er drie tekentaken naast elkaar. Controleer dat venster per app.

5. **`pdga_GameOverDisplayControl.h:508` (`page.enable(true)`) overschrijft de fase-enables van 389-391.** Dat gebeurt vandaag ook (dubbel zelfs, via `show(true)` op 507), dus strikt behoudend vertalen betekent de bug meenemen. Mijn advies is `page.enable(true)` vóór `drawAndEnableWidgetsForPhase(phase)` te zetten, maar dat is een gedragswijziging — laat de auteur beslissen.

6. **`pdga_GameDisplayControl.h:750` maakt de hele boom tekenbaar, inclusief alle 16 balpanelen, crates en vuurkorven** die in de opbouw (571/585/598) juist zijn uitgezet. Dat is vandaag al zo en `applyBallStates()` zet ze een frame later weer uit, maar het betekent dat er bij elke `showPage()` kort zestien ballen op hun startpositie staan. Behouden zoals het is, of samen met deze refactoring opruimen? (`page.makeDrawable(true)` vervangen door alleen de widgets die er horen te staan.)

7. **Blijft `isDrawable()` publiek?** `crt_TestTouchscreenKeyboard.h:207` heeft hem nodig als vervanger van `isShown()`. Verdwijnt hij mee, dan is een eigen `bool bKeyboardShown` in de testtaak de enige overgebleven optie — of `!isEnabled()`, wat eerlijker is zodra hide en disable altijd samen gaan.

8. **`page.show(true)` op een PageRoot blijft alleen veilig zolang niemand binnen die pagina iets bewust niet-tekenbaar zet.** In `pdg`/`pcg` is dat vandaag zo, in `pdga` niet. Overweeg de afspraak "een page-brede `show(true)` bestaat niet, gebruik `makeDrawable` + `redraw`" en handhaaf hem in de commentaarblokken die je toch al herschrijft — dat is precies de klasse fouten waar deze migratie in kan omslaan.

9. **Dekking van de ruwe data.** `apps/pong_cleangui_dg_adv` (3 bestanden, 54 call sites) ontbrak in de aangeleverde inventarisatie; sectie 1.4 is zelf geïnventariseerd en dus niet door dezelfde intentie-analyse heen gegaan als de rest — laat de auteur die tabel nalezen, vooral de negen als **kritiek** gemarkeerde `show(true)` → `redraw(true)`. Volledigheidscontrole voor de rest: een repo-brede grep op `.show(`, `.hide(`, `setDrawable(`, `isShown(`, `.redraw(`, `repaintRegion(`, `redrawRegion(` levert exact de bestanden op die hierboven staan; `libs/CleanGUI_async`, `libs/CleanGUI_extraTests` en `main/` bevatten geen enkele call site.