# pong_dg_adv: modellen als Mermaid (2026-10-19)

- `origineel/`: alle 17 tabs van `apps/pong_cleangui_dg_adv/models/pong_dg_adv.drawio`,
  omgezet met ModelAnalysis `DrawioNaarMermaid/Tools/AllesNaarMermaid` (light mode).
- `bijgewerkt/`: de modellen die de laatste codewijzigingen raken, bijgewerkt. **Groen
  (`#2e7d32`) = nieuw**; de rest staat ongewijzigd zwart op wit.
- `png/`: de zeven bijgewerkte modellen als PNG, op schaal 2. Gerenderd met ModelAnalysis
  `PngNaarMermaid/Tools/testset/render_mermaid.py` (headless Edge, Mermaid 11 + ELK). Dat
  is dezelfde renderer waarmee de testsets van PngNaarMermaid gemaakt worden.
  Uitzondering: het klassediagram staat op schaal 1,5. Op schaal 2 is hij 14056 x 8036 px
  (113 Mpx), en dan schildert headless Edge maar zo'n 64 Mpx: de onderste helft bleef wit,
  zonder foutmelding.

| Bestand | Wat is nieuw |
|---|---|
| `pong_dg_adv_OM-dg-adv.mmd` | objecten `adcButtons`, `buttonInfoControl`, `buttonInfoPageDisplayControl`, `buttonContinue`, plus hun 5 berichten (waaronder `adcButtons → paddleControl: buttonPressed/buttonReleased(buttonIndex)`) |
| `pong_dg_adv_CD-dg-adv.mmd` | klassen `ButtonInfoControl`, `ButtonInfoPageDisplayControl`, `IADC_buttons`, `IADC_buttonsListener`, `ADC_buttons<1>`; nieuwe members in `MainControl` en `PaddleControl` met "(nieuw)" |
| `pong_dg_adv_STD-MainControl.mmd` | begintoestanden `WaitForAdcButtons` (after(200ms)) en `ButtonInfoPage` (do / doButtonInfoPage) |
| `pong_dg_adv_STD-PaddleControl.mmd` | tweede interfaceblok `«interface» IADC_buttonsListener` (S1 = links, S2 = rechts) |
| `pong_dg_adv_STD-ButtonInfoControl.mmd` | nieuw, naar het voorbeeld van STD-StartPageControl |
| `pong_dg_adv_STD-ButtonInfoPageDisplayControl.mmd` | nieuw, naar het voorbeeld van STD-StartPageDisplayControl, zonder fakkels |
| `pong_dg_adv_UC.mmd` | use case `UC09_ButtonInfo`, «include» vanuit `UC01_Main` |

Beperkingen van Mermaid:

- In een state diagram kun je geen losse transities kleuren, en in een klassediagram geen
  losse members of relaties. Elke nieuwe transitie en relatie raakt hier een groene
  toestand of klasse.
- Nieuwe members in bestaande klassen staan in het CD daarom met "(nieuw)", met een
  groene uitleg-note.

Alle zeven zijn met Mermaid 11.4.1 gerenderd en gecontroleerd op fouten en op het groen.

Niet aangepast (staat los van deze wijziging):

- STD-MainControl laat na GameOver nog `StartPage` zien. In de code gaat MainControl
  daar naar `GameRunning`.

## pong_dg_adv_bijgewerkt.drawio

Een bewerkte kopie van `apps/pong_cleangui_dg_adv/models/pong_dg_adv.drawio`, zoals die om
17:55 was opgeslagen; het origineel is niet aangeraakt. Groen (`#2e7d32`) = nieuw. Er is
alleen toegevoegd: bestaande objecten staan nog op hun plek. Nieuwe objecten staan rechts
naast de bestaande inhoud; de layout is nog niet uitgewerkt.

- **UC:** `UC09_ButtonInfo` + «include» vanuit `UC01_Main`.
- **OM-dg-adv:** de vier objecten en vijf berichten die al in het origineel stonden, zijn
  groen gemaakt. Daarbij is de kopie `gameOverDisplayControl` hernoemd naar
  `buttonInfoPageDisplayControl`, en `doButtonInfo` naar `doButtonInfoPage`.
- **CD-dg-adv:**
  - de al begonnen klasse `ButtonInfoPageDisplayControl` heeft de juiste members gekregen,
    is groen en breder (430);
  - nieuw: `ButtonInfoControl`, `IADC_buttons`, `ADC_buttons<1>`, `IADC_buttonsListener` en
    9 relaties;
  - in de bestaande klassen een groene regel bij `MainControl`
    (`- buttonInfoControl : ButtonInfoControl`) en twee bij `PaddleControl`
    (`buttonPressed/buttonReleased(buttonIndex:int)`). Die twee klassen zijn daardoor 20 en
    40 px hoger geworden, op dezelfde plek.
- **STD-MainControl:** nieuwe toestanden `WaitForAdcButtons` → after(200ms) →
  `ButtonInfoPage` → `StartPage`. De bestaande pijl vanuit de begintoestand wijst nu naar
  `WaitForAdcButtons` en is groen.
- **STD-PaddleControl:** een tweede interfaceblok `«interface» IADC_buttonsListener`.
- **Nieuwe tabs** (helemaal groen), direct na STD-StartPageDisplayControl:
  `STD-ButtonInfoControl` en `STD-ButtonInfoPageDisplayControl`.

Gecontroleerd:

- Behalve de genoemde aanpassingen is geen enkele bestaande cel veranderd (per cel vergeleken).
- `AllesNaarMermaid` leest het bestand foutloos: OM 24 objecten / 33 pijlen, CD 37 klassen /
  67 relaties.
- Met draw.io desktop naar PNG geëxporteerd en bekeken.

Nog op te ruimen in de layout:

- `PaddleControl` (CD) is gegroeid en valt nu deels achter `IGameDisplayControl`.
- `ButtonInfoPageDisplayControl` (CD) raakt `PlayButton`.
- In het OM is de naam `buttonInfoPageDisplayControl` iets breder dan zijn vak.
- In het UC staat `UC09_ButtonInfo` vlak naast `UC01_Main`, dus de «include»-pijl is
  bijna onzichtbaar.
- In de nieuwe STD's vallen transitieteksten over elkaar heen (Hidden ↔ Shown) en over de
  toestand `Hidden`.