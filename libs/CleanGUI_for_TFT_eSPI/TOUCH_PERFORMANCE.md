# Touch-performance: een aanpassing in TFT_eSPI die je na een upgrade opnieuw moet doen

**Dit is een lokale wijziging in `libs/TFT_eSPI/Extensions/Touch.cpp`.** Werk je
TFT_eSPI bij, dan is hij weg en komt het probleem terug. Hieronder staat wat je
moet aanpassen en waarom.

## Het verschijnsel

De GUI voelt traag en hertekeningen duren onvoorspelbaar lang, ook als er niemand
het scherm aanraakt. Een volledige hertekening van het toetsenbord kostte
wisselend 65 tot 100 ms, terwijl het tekenwerk zelf constant 43 ms was. Alle
variatie kwam van de touch-polling die ertussendoor liep.

## De oorzaak

`AsyncDisplay` pollt de touch elke 50 ms, in dezelfde taak die tekent. Gemeten:
één `getTouch()` kostte **14 ms** terwijl er niets werd aangeraakt. Dat is bijna
30% van de tijd van de tekentaak, permanent — en het is geen rekenwerk maar
*slapen*, dus het tekenen ligt al die tijd stil.

Waar die 14 ms vandaan komt, gemeten met tellers:

| | |
|---|---|
| `getTouch` roept `validTouch` **5x** aan, onvoorwaardelijk | x5 |
| elke `validTouch` doorloopt de debounce-lus ~2,6x | x2,6 |
| = ~13 rondes per poll, elk met een `delay(1)` | ~10 ms slapen |
| en elk met een `getTouchRawZ()` van 275 us | ~3,6 ms |

Die 275 us per Z-meting is inherent: de touch-SPI is in deze setup bit-banged
(`Touch.cpp`, `touch_spi_transfer`) met twee `delayMicroseconds(1)` per bit.

**Waarom vijf pogingen?** `validTouch` is bewust streng: hij neemt twee
positiemetingen met een pauze ertussen en verwerpt bij drukverlies of als de twee
metingen meer dan `_RAWERR` verschillen. Zo komt ruis er niet doorheen. Maar op
een resistief paneel wiebelt een échte vingerdruk ook, dus één strenge poging zou
veel echte aanrakingen laten vallen. Vandaar vijf pogingen waarvan er één hoeft te
slagen. Dat ontwerp is prima — alleen stopt de lus nergens vroegtijdig, dus het
meest voorkomende geval (niemand raakt aan) betaalt de volle prijs voor een vraag
die na de eerste 275 us al beantwoord was.

## De aanpassing

In `TFT_eSPI::getTouch()` in `libs/TFT_eSPI/Extensions/Touch.cpp`. Origineel:

```cpp
  uint8_t n = 5;
  uint8_t valid = 0;
  while (n--)
  {
    if (validTouch(&x_tmp, &y_tmp, threshold)) valid++;;
  }

  if (valid<1) { _pressTime = 0; return false; }
```

Vervang door:

```cpp
  // (1) Verreweg het vaakst wordt er niets aangeraakt. Een enkele Z-meting kost
  //     275 us en beantwoordt die vraag al.
  if (getTouchRawZ() <= threshold) { _pressTime = 0; return false; }

  uint8_t n = 5;
  uint8_t valid = 0;
  while (n--)
  {
    // (2) De acceptatietoets hieronder is valid<1, dus na een geslaagde poging
    //     staat de uitkomst vast.
    if (validTouch(&x_tmp, &y_tmp, threshold)) { valid++; break; }
  }

  if (valid<1) { _pressTime = 0; return false; }
```

Twee regels, verder niets. De strenge `validTouch` en de vijf pogingen blijven
volledig intact voor een echte aanraking.

## Wat het oplevert (gemeten op de CYD, release-build)

| | voor | na |
|---|---|---|
| `getTouch()`, niets aangeraakt | 14.300 us | **245 us** |
| keyboard-hertekening, wandklok | 65-100 ms | **45,9 ms, stabiel** |

De 245 us is precies één `getTouchRawZ()`. En de hertekening is nu net zo
voorspelbaar als het tekenwerk zelf (43 ms) - de spreiding kwam volledig van de
touch-polls die er wel of niet tussen vielen.

## Het risico, en hoe je het controleert

Uitstap (1) leest de druk één keer in plaats van te wachten tot die niet meer
stijgt. Begint een aanraking net op het moment van de poll, dan kan die ene
meting nog onder de drempel zitten en wordt de druk pas bij de volgende poll
gezien - 50 ms later. Voor een vingerdruk, die honderden milliseconden duurt, is
dat onzichtbaar. Bij een zeer korte tik zou het in theorie kunnen schelen.

In de praktijk is de marge groot: onaangeraakt geeft `getTouchRawZ()` 0 (de code
zet 4095 om naar 0), en een echte druk zit ver boven de drempel. Het twijfelgebied
is dus smal.

Controleer na het opnieuw aanbrengen met het `Inputfield`-voorbeeld:
- tikt het invoerveld betrouwbaar aan (toetsenbord schuift omhoog)?
- registreert elke toetsaanslag op het toetsenbord, ook snelle?
- werkt ENTER (toetsenbord schuift terug)?

Uitstap (2) is gedragsneutraal voor alle aanroepers in dit project: die gebruiken
de returnwaarde als `bool`, niet als telling. `TFT_eSPI_TouchDetector::update()`
doet `bool bPressed = _tft.getTouch(&t_x, &t_y);`.

## Alternatief zonder TFT_eSPI aan te raken

Wil je liever niet van upstream afwijken, dan kun je de touch-polling in een eigen
taak zetten in plaats van in de AsyncDisplay-taak. De 14 ms blijft dan bestaan,
maar blokkeert het tekenen niet meer. Dat is een grotere ingreep - er komt een
taak bij en de touch-listeners draaien dan in een andere context - en de CPU-tijd
blijft verspild. De twee regels hierboven zijn goedkoper.
