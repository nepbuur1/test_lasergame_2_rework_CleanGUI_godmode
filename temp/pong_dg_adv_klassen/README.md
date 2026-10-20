# pong_dg_adv: OM en CD naast de klassen in de code (2026-10-20)

Ik heb de tabs `OM-dg-adv` en `CD-dg-adv` van
`apps/pong_cleangui_dg_adv/models/pong_dg_adv.drawio` vergeleken met de klassen en structs die in
`apps/pong_cleangui_dg_adv/*.h` gedeclareerd worden. Klassen van elders (CleanRTOS, CleanGUI,
CYD_tools_crt: `ADC_buttons`, `TouchscreenButton`, `Inputfield` enzovoort) heb ik niet nagelopen.

Bestanden:

- `pong_dg_adv_origineel.drawio`: een ongewijzigde kopie van het origineel van dit moment.
- `pong_dg_adv_klassen.drawio`: de aangepaste kopie. Het origineel in `apps/` is niet aangeraakt.
- `mermaid/`: OM en CD als Mermaid (via ModelAnalysis `AllesNaarMermaid`), zowel origineel als
  bijgewerkt.
- `png/`: de twee bijgewerkte tabs, geëxporteerd met draw.io desktop.

## Kleuren

- **Groen (`#2e7d32`):** nieuw of aangepast. Nieuwe cellen en pijlen zijn groen via hun style
  (`strokeColor`/`fontColor`). In bestaande tekstcellen staan alleen de aangepaste regels in
  `<font color="#2e7d32">`, zodat je ziet welke regel veranderd is.
- **Rood (`#c62828`):** staat in het model, maar bestaat niet in de code. Ik heb niets
  verwijderd; jij beslist.

Er is niets verplaatst. Wel zijn klassen met extra regels breder of hoger geworden: x en y
blijven gelijk, en hun regelvakken schuiven binnen de klasse naar beneden. Layout volgt.

## CD-dg-adv

### Nieuwe klassen

Alle structs en interfaces uit de map die nog in het CD ontbraken:

- `HallOfFameData`, `INameInputPageListener` en `CrateHit`;
- `BallState`, `BallStates`, `CrateState`, `CrateStates`, `FireBinState` en `FireBinStates`;
- met relaties: compositie, realisatie en afhankelijkheid.

### Aangepast in bestaande klassen

| Klasse | Was | Nu (volgens de code) |
|---|---|---|
| GameOverPageControl | naam | `GameOverControl` |
| GameOverControl | `- gameOverDisplayControl : IGameOverDisplayControl&` | `- hallOfFameDisplayControl : HallOfFameDisplayControl&` |
| StartPageControl | `IStartPageDisplayControl&` | `StartPageDisplayControl&` |
| StartPageDisplayControl | `buttonSpStart` | `buttonStart`, plus `addStartPageStartButtonListener(...)` |
| HallOfFame | `+ arEntry : HallOfFameEntry[5]` | `- hallOfFameData : HallOfFameData`, plus `getHallOfFameData()`, `clear()`, `load()` |
| HallOfFameDisplayControl | `buttonGoStart : ButtonGoStart`, `showHallOfFame(...)`, `hideHallOfFamePage)(` | `buttonPlayAgain : PlayAgainButton`, `showHallOfFamePage(hallOfFameData)`, `hideHallOfFamePage()`, `addPlayAgainButtonListener(...):IButton*` |
| NameInputDisplayControl | een kopie van de members van HallOfFameDisplayControl | de eigen members en operaties |
| NameInputControl | `IGameOverDisplayControl&` | `- nameInputDisplayControl : NameInputDisplayControl&` |
| GameControl | `IGameDisplayControl&`, `Spawnables&` | `GameDisplayControl&`, `Spawnables` (by value) |
| PaddleControl | `- displayControl : IGameDisplayControl&` | `- gameDisplayControl : GameDisplayControl&` |
| GameDisplayControl | `ButtonLeft` / `ButtonRight` / `ButtonGameOverLabel` | `TouchscreenButton<0,10>`, plus de drie `add…Listener(...)` |
| Spawnables | `IGameDisplayControl&` | `GameDisplayControl&`, plus `ballStates`/`crateStates`/`fireBinStates`, `setBallListener`, `getNofCratesInUse`, `getNofFireBinsInUse` |
| BallControl | `IGameDisplayControl*`, `init(..., pBallField:IBallField*, ...)` | `GameDisplayControl*`, `init(..., pBallCollisionCheck:IBallCollisionCheck*, ...)` |
| CrateControl | | `+ isOpened():bool` erbij |
| FireBinControl | | `+ getPosX()`, `+ getPosY()` erbij |
| ButtonInfoDisplayControl | `showButtonInfoPage()` | `showButtonInfoPage(bConnected:bool)`, plus `addButtonInfoContinueButtonListener(...)` |
| IDepot (2x) | `release()` | `release(index:int)` |
| IBallSpawner | `nofBalls` | `nofBalls:int` |
| IBallCollisionCheck | `crateWasHitAtPos(hit:CrateHit&)` | `(hit:const CrateHit&)` |
| IDepotItem | `update` | `update()` |

### Pijlen

- Omgezet naar `GameDisplayControl`: de associaties van `GameControl`, `PaddleControl`,
  `Spawnables` en `BallControl` die naar `IGameDisplayControl` wezen.
- Omgezet naar `HallOfFameData`: `HallOfFame *-- HallOfFameEntry` is nu
  `HallOfFameData *-- HallOfFameEntry`, en `HallOfFameDisplayControl ..> HallOfFame` is nu
  `..> HallOfFameData`.
- `ButtonInfoControl → ADC_buttons<1>` wijst nu naar `IADC_buttons`, het type van de member.
- De compositie tussen `ButtonInfoDisplayControl` en `PlayButton` stond andersom (de ruit zat
  bij PlayButton); omgedraaid.
- Het losse label "Text" op `ButtonInfoControl → ButtonInfoDisplayControl` is leeggemaakt.
- Nieuwe pijlen:
  - `IDepot <|.. Depot<FireBinControl, …>` (die realisatie ontbrak);
  - een zichtbare `FireBinControl → IDepot`. De bestaande lijn had `strokeColor=none` en was
    dus onzichtbaar.
  - `IInputfieldListener <|.. NameInputDisplayControl`;
  - `INameInputPageListener <|.. NameInputControl` en
    `NameInputDisplayControl → INameInputPageListener`.

### Rood

- `IGameDisplayControl` bestaat niet in de code. Alle klassen gebruiken `GameDisplayControl&`
  rechtstreeks. Na het omzetten van de pijlen hangt alleen de realisatie er nog aan.

### Bewust niet aangevuld

- Methoden die een klasse alleen implementeert omdat een interface ze voorschrijft, zoals
  `buttonReleased` en `reset()`. Die volgen al uit de realisatiepijl; zo deed je het in het
  CD ook al.
- Constructors, private hulpfuncties, flags, timers, pools en enums.

## OM-dg-adv

| Wat | Was | Nu |
|---|---|---|
| object | `buttonSpStart` | `buttonStart` |
| object | `buttonGoStart` | `buttonPlayAgain` |
| object | `gameOverPageControl` | `gameOverControl` |
| object | `gameOverDisplayControl` (bij gameOverPageControl) | `hallOfFameDisplayControl` |
| object | `gameOverDisplayControl` (bij buttonInfoControl) | `buttonInfoDisplayControl` |
| object | `nameInputfield` | `inputfield` (member van nameInputDisplayControl) |
| stereotype | `nameInputControl` «boundary» | «control» |
| bericht | `doButtonInfo` | `doButtonInfoPage` |
| bericht → hallOfFameDisplayControl | `showGameOverPage, hideGameOverPage, showHallOfFame(hallOfFame)` | `showHallOfFamePage(hallOfFameData), hideHallOfFamePage` |
| bericht → hallOfFame | | `getHallOfFameData` erbij |
| bericht spawnables → ballControl | `init(gameDisplayControl,pBallField,…)` | `pBallCollisionCheck` |
| bericht gameControl → spawnables | | `updateBalls`, `updateCratesAndFireBins`, `publishStates(score)`, `setBallListener`, `getNof…InUse` erbij |
| bericht gameControl → paddleControl | | `getPaddlePosition` erbij |
| bericht inputfield → nameInputControl | | `inputfieldPressed`, `outsideInputfieldPressed` erbij |

Nieuw:

- het object `nameInputDisplayControl`, met de berichten:
  - `showNameInputPage, hideNameInputPage` vanuit nameInputControl. Deze pijl wees eerst met
    `showNameInput` naar gameOverDisplayControl.
  - `nameInputPageHidden` naar nameInputControl;
  - `clearInput` naar inputfield;
  - `inputNewKey`, `inputfieldPressed` en `outsideInputfieldPressed` vanuit inputfield.

Rood:

- `hallOfFameDisplayControl → hallOfFame: getEntry(index)`. De display krijgt de gegevens via
  `showHallOfFamePage(hallOfFameData)` en roept zelf niets aan op hallOfFame.

De depots staan nog steeds niet als objecten in het OM. spawnables roept de balls, crates en
firebins via de depots aan. Dat is een vereenvoudiging, dus niet aangepast.

## Gezien, maar buiten deze opdracht

Deze punten gaan over klassen van elders of over andere tabs:

- In het CD wijst de realisatiepijl tussen `ADC_buttons<1>` en `IADC_buttons` de verkeerde kant
  op: de driehoek staat bij ADC_buttons.
- `ADC_buttons<1>` heeft het stereotype «interface», maar het is een klasse.
- In de noten bij PlayButton en PlayAgainButton, en in de klasse `TouchScreenButton <1,10>`,
  staat `<1,10>`. De typedefs in de code zijn `TouchscreenButton<0,10>`.
- `NameInputfield` is in het CD een eigen klasse. In de code is het
  `Inputfield<0,2,MaxNofNameFieldChars>` uit CleanGUI.
- `Depot<T,N>` realiseert ook `IHandlerListener` (CleanRTOS). Dat staat niet in het CD.
- De tab `ButtonInfoDisplayControl` (de STD) heeft geen `STD-` voor zijn naam. Daardoor
  slaat `AllesNaarMermaid` hem over.
- `AllesNaarMermaid` neemt onzichtbare hulplijnen (`strokeColor=none`) op als echte relaties.
  In de originele mermaid staat daardoor `CrateControl --> FireBinControl`, maar die relatie
  bestaat niet.
