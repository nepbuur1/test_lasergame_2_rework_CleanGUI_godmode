# TestWorstCaseVoltageToBorderDistance

Een test op de hardware: hoe ver ligt de gemeten spanning van elke ingedrukte
knoppencombinatie van de dichtstbijzijnde beslisgrens af? Een beslisgrens is de
drempel naar een andere combinatie. De uitkomst geldt per combinatie, in mV, als
worst case over alle metingen.

## Draaien

In `main/main.cpp` als enige `#include` actief:

```cpp
#include <TestWorstCaseVoltageToBorderDistance.ino>
```

Dan `idf.py build`, `idf.py -p /dev/ttyUSB0 flash monitor`. Druk na de melding
"Klaar." zoveel mogelijk combinaties in en houd ze elk ±1 s vast. Met de standaardinstelling
(hooguit 3 knoppen tegelijk) zijn dat er 41: 6 enkele, 15 paren en 20 drietallen. Laat alles 15 s los voor de tabel.

## Uitvoer

Per losgelaten combinatie één regel, bijvoorbeeld:

```
S1+S3          code 40:  92 metingen (+3 uitschieters), mediaan  757 mV, afstand mediaan   9.8 / kleinste   6.8 mV tot de grens erboven (nominaal 16.4)  |  gezien  7/41, slechtste: 5.2 mV bij S1+S6
```

Elke 5 s staan er 5 combinaties in de log die nog niet gedrukt zijn, eerst die met de
minste knoppen:

```
>>> Nog 23 te gaan. Druk:  S1+S2+S4,  S1+S2+S5,  S1+S2+S6,  S1+S3+S4,  S1+S3+S5
```

Kijk daarvoor live mee met `idf.py -p /dev/ttyUSB0 monitor`.

Na 15 s zonder drukken volgt een tabel van alle geziene combinaties, de slechtste
eerst. Per combinatie staan erin:

- hoe vaak hij is ingedrukt;
- het aantal steady metingen en het aantal uitschieters;
- de laagste en hoogste steady spanning;
- de kleinste afstand van een steady meting tot een grens, en welke grens (erboven
  of eronder): **de worst case**;
- de kleinste afstand van de mediaan van een druk;
- de nominale afstand volgens de berekening.

Een negatieve afstand betekent dat de spanning aan de verkeerde kant van een grens lag.

## Hoe het meet

- Elke 10 ms één snapshot van `ADC_buttons::readSnapshot`: de gedebouncede code,
  de gemeten spanning (gemiddeld over 32 samples) en de gekalibreerde Vref. Alle drie
  komen uit dezelfde toestand.
- Een druk is een periode waarin de gedebouncede code gelijk blijft.
  - De eerste 50 ms en de laatste 80 ms tellen niet mee. Aan het begin zet het contact
    zich nog; aan het eind verandert de spanning al, terwijl de debounce de oude code
    nog vasthoudt.
  - Blijft er minder dan 5 metingen over, dan was het een tussenstand en telt de druk
    niet mee.
- **De mediaan van een druk bepaalt de combinatie**: de combinatie in wier band hij valt.
  Dat hoeft niet de gedebouncede code te zijn. Ligt een combinatie precies op een grens,
  dan springt de debounce soms niet mee, en anders zou je de spanning van de ene
  combinatie aan de andere toerekenen. Wijkt de mediaan af van de gedebouncede code, dan
  staat er "LET OP: herkend als ..." in de regel.
- Metingen die meer dan 10 mV van de mediaan liggen, zijn dender of een overgang. Ze tellen
  niet mee voor de afstand, maar staan wel als uitschieters in de regel en de tabel.
- De grenzen komen uit `ADC_buttons_E12_ladder::getUpperBorder/getLowerBorder`. Die
  liggen midden tussen twee opeenvolgende toegestane niveaus. Ze worden met de
  gekalibreerde Vref omgerekend naar mV.
- `maxSimultaneousButtons` staat in deze test op 3, net als de default van
  `ADC_buttons`. Er worden dan alleen combinaties van hooguit 3 knoppen herkend, en
  de grenzen liggen midden tussen hun niveaus. Zo meet de test de marges die een
  applicatie krijgt. Zet hem op 6 om de ladder zelf te meten (alle 64 combinaties);
  zo is de referentie in `results/2026-10-19_E12_RL68k.md` gemeten.
- "Geen knop" telt niet mee: de zelfkalibratie zet dat niveau per definitie op zijn
  nominale waarde.

## Let op

De test weet niet wat je bedoelde in te drukken. Valt de mediaan van een druk in de
band van een andere combinatie dan je indrukte, dan telt hij mee bij die andere, met
een kleine afstand. Controleer dus in de log dat elke combinatie herkend wordt als wat
je indrukte.

Met `maxSimultaneousButtons` = 6: combinaties van vier of meer knoppen met S1..S4 erin
liggen onder ±100 mV, waar de ESP32-ADC nul leest. Die zijn sowieso niet betrouwbaar te
onderscheiden. Druk je met de standaardinstelling vier of meer knoppen tegelijk in, dan
telt dat als het dichtstbijzijnde drietal (of minder).

De ladderwaarden (B, P, RL) komen uit `src/ADC_buttons/crt_ADC_buttons_E12_ladder.h`.
Na het ombouwen van de ladder pas je ze daar aan en draai je de test opnieuw.
