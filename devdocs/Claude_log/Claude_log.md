# Claude log

## 2026-08-12 — Touchkalibratie per schermrotatie, en de nachtopdracht naar git

Twee dingen, en het tweede is de reden voor het eerste.

### Terugdraaien

De nachtelijke poging van 11 augustus op `refactor/cleangui-draw-enable-split`
(3 commits plus ongecommit werk) is teruggedraaid: branch weg, `main` weer op
`13f7254`, schone boom. Het werk is niet vernietigd maar staat lokaal onder de
tag `backup/nachtwerk-2026-08-11`, met het ongecommitte deel als extra
WIP-commit erbovenop. Die tag is niet gepusht.

### Waarom die poging niet zelfstandig af te maken was

Elke app en elk voorbeeld kiest zijn eigen schermrotatie. De kalibratie werd wél
mét rotatie bewaard, maar er paste er **maar één** in `/TouchCalData1`:

```
[0]      rotatie
[1..10]  5 uint16 calData
```

Startte je een app in een andere rotatie, dan kwam die byte niet overeen, werd er
opnieuw gekalibreerd, en werd het bestand overschreven — de kalibratie van de
vorige rotatie was daarmee weg. Achter elkaar door de voorbeelden lopen kostte dus
elke flash vier hoeken aantikken, en dat is precies wat een autonome run niet kan.

Erger nog was de sloop bij een *gewone* hercalibratie: bij een rotatiemismatch en
bij de "Touch now to recalibrate"-vraag deed de oude code eerst
`LITTLEFS.remove()`. Wat er voor andere rotaties in stond ging daar sowieso bij weg.

### Wat er nu staat

Formaat v2 in hetzelfde bestand, 46 bytes:

| offset | inhoud |
|---|---|
| 0..3 | magic `'C','G','T','C'` |
| 4 | versie (2) |
| 5 | geldigheidsmasker, bit *r* hoort bij rotatie *r* |
| 6..45 | 4 slots van 5 uint16, little endian |

De twee versies zijn niet te verwarren: v1 is 11 bytes en begint met een rotatie
(0..3), v2 is 46 bytes en begint met `'C'` (67). Een v1-bestand wordt bij het lezen
overgenomen in het slot waar het volgens zijn eigen rotatiebyte hoort, dus een bord
dat al gekalibreerd was hoeft die ene rotatie niet opnieuw te doen.

Gedrag, en dat is bewust asymmetrisch:

| wie | wat |
|---|---|
| `touchCalibrate(rotation, font)` — elke app | leest het slot van *zijn eigen* rotatie. Staat het er, dan `setTouch` plus de bestaande 2-secondenvraag "Touch now to recalibrate". Wordt daarop getikt, dan wordt **alleen dat ene slot** opnieuw gevuld |
| `touchCalibrateAllRotations(font)` — nieuw | kalibreert 0, 1, 2, 3 achter elkaar en bewaart na élke rotatie |

`LITTLEFS.remove()` is helemaal verdwenen; `FILE_WRITE` kapt het bestand toch af en
de hele tabel gaat er in één keer opnieuw in. Per rotatie opslaan en niet pas aan
het eind: gaat het bord halverwege uit, dan is alleen de laatste rotatie kwijt.

Tussen twee rotaties door zit een `waitUntilTouchReleased()`. `calibrateTouch()`
last pas ná de eerste hoek een pauze in (`if (i>0) delay(1000)`), terwijl de vinger
op dat moment nog op de hoek rechtsonder van de vórige rotatie ligt. Die wacht kijkt
naar `getTouchRawZ()` en niet naar `getTouch()`, want dat laatste rekent de meting
eerst om met de kalibratie die er op dat moment staat — en juist die is daar nog
onbetrouwbaar.

Nieuw voorbeeld `libs/CleanGUI_for_TFT_eSPI/examples/TouchCalibrateAll`, bewust
zonder AsyncDisplay en zonder widgets, op de RGB565-adapter: de kalibratie tekent
rechtstreeks op het paneel en draait voordat er een widgetboom is. De PAL8-variant
zou per rotatie een backbuffer van een andere maat nodig hebben, en er valt hier
niets te bufferen.

### Past het in de partitie?

Nagerekend voor alle apps en examples, want het bestand groeit van 11 naar 46 bytes.

De `spiffs`-partitie uit `partitions.csv` is `0x19000` = 102.400 bytes, en littlefs
werkt op de ESP32 in blokken van 4096 bytes (`CONFIG_LITTLEFS_BLOCK_SIZE`), dus 25
blokken. Deze codebase zet er in totaal **twee** bestanden in:

| bestand | wie | grootte |
|---|---|---|
| `/TouchCalData1` | elke app en elk example, via de display-adapter | 11 → **46** bytes |
| hall of fame | alleen `pong_cleangui_dg_adv` | `1 + 5 * (20 + 1 + 4)` = 126 bytes |

Beide liggen ruim onder de inline-grens van littlefs — `cache_size` is 512 en
`block_size/8` is eveneens 512 — dus ze worden in de metadata van de rootdirectory
opgeborgen en kosten geen eigen datablok. Zelfs als ze dat wél deden: twee blokken
plus het superblok-paar is 16 KB van de 100 KB. De 35 bytes die deze wijziging
toevoegt kosten op blokniveau dus letterlijk niets.

Twee kanttekeningen die geen ruimteprobleem zijn maar wel het bestand raken:

- De TFT_eWidget-voorbeelden `Button_demo.ino` en `Slider_demo.ino` schrijven naar
  hetzelfde pad `/TouchCalData1`, in hun eigen indeling van 14 bytes. Ze staan niet
  in `main.cpp` aan. Draai je er toch een, dan is de tabel weg en moet je
  `TouchCalibrateAll` opnieuw doen. Het leesfilter eist daarom **exact** 11 bytes
  voor een v1-bestand en niet "minstens", anders zou zo'n bestand van 14 bytes soms
  als v1 worden aangezien — de eerste byte ervan is de LSB van `calData[0]` en die
  kan toevallig onder de 4 liggen.
- `mountFileSystem()` logt sinds nu één keer per boot
  `littlefs: <used> van <total> bytes in gebruik`, zodat dit op het bord na te
  rekenen is in plaats van alleen op papier.

### De opdracht zelf staat nu in git

`devdocs/Claude_instructions/CleanGUI_show_enable_refactor.md` — de volledige
specificatie van de show/enable/draw-refactor, inclusief de redenering achter
`makeDrawable`/`makeNonDrawable`, waarom `_bVisible` weg kan, waarom `hide()` op
`repaintRegion` moet en niet op een `fillRect` met de achtergrondkleur, het
stappenplan 0-10, en hoe het zonder toezicht te testen valt (touch namaken via
`ITouchListener`, pixels teruglezen via de PAL8-buffer). Bedoeld om na een
`git pull` op een andere machine uitgevoerd te worden.

Wat daar ook in staat als valkuil: `apps` staat in `.claudeignore`, terwijl vier
van de tien stappen juist in `apps/` zitten.

## 2026-08-07 — GameObjectsControl uit elkaar: opslag, beeldklok, en drie interfaces van een regel

`GameObjectsControl` deed vier dingen: het hield de depots vast, het beantwoordde de
relatievragen van de ballen, het bewaarde de score, en het was de `IHandlerListener`
die per beeld de volgorde bepaalde en publiceerde.

Dat was aan de buitenkant te zien, en dat is de reden dat het opviel. `GameControl`
praatte deels rechtstreeks tegen de depots (`ballDepot.obtain()`,
`crateDepot.releaseAll()`, drie `init`-lussen in zijn constructor) en deels via
`GameObjectsControl` (`spawnCrateAt`, `getNofBallsInUse`, `resetScore`). Wie iets aan
een spelobject wilde, moest weten via welke van de twee.

### De verdeling

| | |
|---|---|
| `Spawnables` | bezit de drie depots en de States-arrays; beantwoordt de relatievragen (`IBallField`); publiceert de standen |
| `SpawnablesHandler` | de beeldklok: de volgorde per beeld, en het signaleren van de overgang naar nul ballen |
| `GameControl` | bezit de score |

`GameControl` heeft nu geen enkele depot-verwijzing meer - grep op `ballDepot` daar
geeft nul treffers - en de `init`-lussen staan bij de eigenaar van de depots, waar ze
`this` meegeven in plaats van een doorgegeven pointer.

Dat de beeldklok een eigen object werd en geen methode van `Spawnables`, is om de
volgorde. Daar zitten afspraken in die je aan de losse depots niet kunt zien -
peddelstand voor alles beweegt, korven en crates voordat de ballen kijken of ze er een
raken - en zulke afspraken horen op een plek te staan waar ze te lezen zijn.

De score naar `GameControl` had een addertje: `publishStates()` heeft hem elk beeld
nodig, want hij gaat mee in hetzelfde beeldbericht als de standen. De handler haalt
hem daarom op met `getScore()` en geeft hem als parameter mee aan `publishStates()`.
Dat is bewust geen listener-aanroep binnen publishStates: een publiceerfunctie die
zelf ergens een waarde gaat halen die de aanroeper al heeft, is een omweg.

Het lid `score` woont daarmee in een object waarvan de eigen taak die van
`MainControl` is, terwijl het wordt opgehoogd en gelezen vanuit de Handler-taak. Geen
Pool nodig - een uitgelijnde `int32_t` is op de ESP32 ondeelbaar en een beeld verschil
op het scherm is niet te zien - maar wel een opmerking bij het lid, want zonder uitleg
leest het als een gewone member.

### En daarna nog een slag: Spawnables was nog te veel

Opslag met gemak eromheen, en niets meer - dat was het doel, en `crateWasHitAtPos()`
haalde dat niet. Die telde de crate af, spawnde tien ballen en gaf het slot vrij.

Dat werk zit nu in `CrateControl::wasHit()`, waar het hoort: een crate heeft zijn
teller en zijn plek zelf, en zichzelf vrijgeven met `pDepot->release(indexSlot)` is
hetzelfde patroon als een bal die het scherm verlaat. Voor de ballen die eruit komen
is er `IBallSpawner`, want een crate hoort niet te weten dat er zoiets als een
baldepot bestaat. Wat er bij `Spawnables` overblijft is opzoeken en doorgeven:

```cpp
/*override*/ void crateWasHitAtPos(const CrateHit& hit)
{
    if (!hit.bHit || !crateDepot.isInUse(hit.indexSlot)) { return; }
    crateDepot.getItem(hit.indexSlot).wasHit();
}
```

De bal meldt zijn peddelkaatsing nu rechtstreeks aan `GameControl`, via
`IBallListener`, in plaats van via het veld waarin hij ligt. Daarmee is `IBallField`
een methode korter en heeft `Spawnables` helemaal geen luisteraar meer: geen
doorgeefwerk, want opslag hoort geen boodschappen te bezorgen.

Drie nieuwe headers, elk met een methode: `IBallSpawner`, `IBallListener` en
`IGameListener`. Die laatste moest los omdat hij eerst in `pdga_Spawnables.h` stond en
`Spawnables` op zijn beurt `BallControl` includeert - een kringetje zodra de bal hem
nodig heeft.

Een ding dat het plan niet voorzag: de ballen worden bedraad in de constructor van
`Spawnables`, en `GameControl` bestaat dan nog niet - die krijgt `Spawnables` juist
mee. Vandaar `Spawnables::setBallListener()`, dat de lus over de ballen doet.
Bedrading van eigen exemplaren, in dezelfde geest als de `init`-lussen ernaast, maar
wel een setter meer dan bedacht.

### Namen in de spawn-familie

`BallControl` had `spawnAt(x,y)` terwijl crate en korf `spawn(x,y)` heetten, met net
zo goed een plek erin. Nu heet alles waar een positie in gaat `spawnAt`, wat ook
aansluit op `spawnCrateAt` / `spawnFireBinAt` / `spawnBallsAt` bij `Spawnables`.

En `BallControl::spawn()` kon weg: die deed niets anders dan `spawnInTheMiddle()`
aanroepen. Over blijven twee spawn-functies die allebei in hun naam zeggen waar de bal
terechtkomt, en geen naamloze derde ertussen. Het commentaar erboven ("aan te roepen
nadat het depot deze bal heeft uitgegeven") gold voor allebei en staat nu boven het
paar.

### Wat het bord ervan vond

Twee testrondes, geen enkele `E`- of `W`-regel, geen assert.

Na de splitsing: scores van 41 en 4 punten. De 41 zegt dat het spel niet te vroeg
eindigde - dat is de nul-ballen-check die naar de handler verhuisde, en had die de
*stand* gemeten in plaats van de *overgang*, dan was elk potje afgelopen voor de eerste
bal. En 4 na 41 zegt dat `score = 0` bij het begin van een spel werkt.

Na de tweede slag werd het overtuigender:

```
I (59003) Depot: crateDepot: alle 5 exemplaren zijn in gebruik.
I (60562) Depot: ballDepot: alle 50 exemplaren zijn in gebruik.
I (61274) pong: 26 fps
I (73606) pong: top-5: 'hhh' met 69 punten opgeslagen
```

Een uitgeput baldepot betekent minstens vijf crates die hun hele `wasHit()` hebben
doorlopen - aftellen, tien ballen, zichzelf vrijgeven - en dat een laag dieper dan
waar die code stond. Was het vrijgeven blijven hangen, dan waren de crateslots vol
gebleven en had het baldepot nooit vijftig hoeven uitgeven.

En 26 fps dwars door vijftig ballen heen: over de hele capture 133 metingen van 26,
een van 27 (meetjitter) en een van 13, en die laatste is de eerste meting na het
opstarten. Geen inzakking tijdens het spel.

### Nog open

- `models/pong_dg_adv.drawio` is niet meegegaan. Daar staat nog `GameObjectsControl`
  met `qualifies(score)`, en na twee dagen hernoemen loopt dat verder achter dan die
  ene pijl.
- `Widget::enable(bool)` neemt `bIncludeChildren` en geen aan/uit; zie de entry
  hieronder.
- De andere twee apps (`pong_cleangui_dg`, `pong_cleangui_cg`) hebben van dit alles
  niets meegekregen.

## 2026-08-07 — Van een rauwe aanwijzer naar een naamgevingsconventie

Begonnen met een vraag over een aanwijzer, geeindigd met een regel voor de hele app.
Die volgorde is geen toeval: de tweede vraag kwam pas boven omdat de eerste het
patroon zichtbaar maakte.

### De peddelstand loopt nu via IBallField

`BallControl` las de peddelstand via `const int* pPaddlePosition`, een aanwijzer naar
een lid van `GameObjectsControl` dat werd uitgedeeld door `getPaddlePositionPtr()`.

De reden om via het veld te gaan en niet rechtstreeks naar `PaddleControl` blijft
overeind, en die is niet klein: `getPaddlePosition()` daar gaat via een `Pool` met een
SimpleMutex, dus met vijftig ballen waren dat vijftig slotoperaties per beeld op een
slot dat PaddleControl (prioriteit 1) zelf ook neemt - de Handler (prioriteit 2) kon
daarop gaan wachten. Het veld haalt de stand daarom een keer per beeld op, in dezelfde
taak waarin de ballen daarna hun `update()` doen.

Alleen verklaarde dat de *aanwijzer* niet. De bal houdt `GameObjectsControl` al vast,
als `IBallField*`. Een getter op die interface levert precies hetzelfde - geen mutex,
een waarde per beeld, alle ballen dezelfde peddel - en kost een virtuele aanroep per
bal per beeld, zo'n 1300 per seconde bij 26 fps. Dat is een andere orde dan vijftig
mutexen, en het scheelt een rauwe aanwijzer naar andermans lid waarvan de header moest
beloven dat hij geldig bleef.

De redenering hierboven is meeverhuisd naar `IBallField::getPaddlePosition()`. Dat was
het punt om op te letten: die uitleg is het waardevolste in dit hoekje van de code en
mocht niet met de aanwijzer meeverdwijnen.

### De conventie

| soort | vorm | voorbeeld |
|---|---|---|
| booleaanse vraag | `is...` | `isFireBinTouchedAtPos(x, y, r)` |
| vraag, instantaan antwoord | `get...` | `getCrateHitAtPos(x, y, r)` |
| vraag die op iets moet wachten | `query...` | `queryName` |
| idem, synchrone communicatie | `do...` | `doQueryName()` |
| mededeling | klinkt als een mededeling | `crateWasHitAtPos(hit)` |

Aanleiding: `overlaps()`, `touchesFireBin()` en `qualifies()` waren drie booleaanse
vragen in drie vormen. De `do`-vorm stond er al goed op (`doGame`, `doStartPage`,
`doGameOver`, `doQueryName`) en `get` klopte bijna overal.

Een keuze die de conventie niet zelf maakt: **een opdracht die alleen slaagt-of-niet
teruggeeft is geen vraag.** `spawnCrateAt()`, `spawnFireBinAt()` en
`beginFileSystem()` retourneren `bool` maar vragen niets - je laat iets doen en hoort
of het lukte. Die blijven gebiedend; `isSpawnCrateAt()` zou een leugen zijn.

### reportCrateHit was geen naam maar twee functies

De vraag die dit losmaakte: waarom is `reportCrateHit` anders van vorm dan
`touchesFireBin`? Twee redenen deugden - de bal moet tegen een crate kaatsen en heeft
daar plek en maat voor nodig, en een crate-treffer verandert de crate terwijl een korf
niets ondergaat. Maar er was een derde, en die was de echte: `reportCrateHit` muteerde
*en* antwoordde. Vandaar dat geen enkele naam paste, en dat de aanroepplek als een
vraag las terwijl de declaratie `report` zei.

Uit elkaar getrokken valt elke helft onder een regel:

```cpp
virtual CrateHit getCrateHitAtPos(int x, int y, int r) = 0;  // zoekt, verandert niets
virtual void     crateWasHitAtPos(const CrateHit& hit) = 0;  // telt af, opent bij nul
```

`CrateHit` kreeg het slotnummer erbij, zodat de mededeling de crate niet voor de
tweede keer hoeft te zoeken. Twee stukken commentaar konden daardoor weg of verhuizen:
"de plek onthouden voordat de crate wordt vrijgegeven" is overbodig geworden - de
vraag geeft de bal al een kopie voordat er iets vrijgegeven kan worden - en de uitleg
over herentrantie is mee naar `crateWasHitAtPos()`, waar de mutatie nu zit. Allebei
bestonden ze alleen omdat vragen en veranderen in een aanroep zaten.

Verder hernoemd: `overlaps` werd `isHitAtPos` op de crate en `isTouchedAtPos` op de
korf (dat onderscheid zit al in UC05 stap 1c tegen 1d), `registerHit` werd `wasHit`,
`reportPaddleBounce` werd `ballBouncedOnPaddle`, `qualifies` werd `isQualifyingScore`,
en `PaddleControl::getPaddlePos` heet nu net als zijn tegenhanger `getPaddlePosition`.

`ballBouncedOnPaddle()` is als enige zonder coordinaten, en dat verdient uitleg in
plaats van dat het als slordigheid leest: de bal doet de peddelmeetkunde zelf, met
`getPaddlePosition()`, dus hij meldt wat hij al weet.

Buiten bereik bleven de namen die uit `libs/` komen - `buttonPressed`, `inputNewKey`,
`IHandlerListener::update()`, `IDepot::release()` - want die hernoemen raakt de
bibliotheek en alle andere apps. `buttonPressed` en `buttonReleased` voldoen toevallig
al.

### Wat het bord ervan vond

Twee testrondes, geen enkele `E`- of `W`-regel, geen assert, geen herstart.

Voor de peddelwijziging: scores van 2, 2 en 5 punten. Score komt uitsluitend van
peddelkaatsingen, dus negen keer vond de bal de peddel waar die stond - was de
bedrading stuk geweest, dan was `paddlePosition` nul en had geen enkel potje een punt
opgeleverd.

Voor de splitsing: `top-5: 'gg' met 52 punten`. Zo'n score haal je niet zonder een
crate open te spelen, en dat levert tien ballen ineens. Beide helften werken dus:
zonder de mededeling zou er nooit een crate opengaan, en telde hij dubbel dan waren de
kistjes na de helft van de treffers al opengesprongen.

En het punt waar de aanwijzer ooit voor bestond: `Depot: crateDepot: alle 5 exemplaren
zijn in gebruik`, met 26 fps ervoor, eromheen en erna. 78 van de 84 fps-metingen waren
26; de rest zit bij het opstarten en de eerste beeldmeting na een paginawissel, die
altijd laag uitvalt.

### Nog open

- `models/pong_dg_adv.drawio` noemt nog `qualifies(score)` in de pijl naar
  `HallOfFame`, en waarschijnlijk meer namen die nu verschoven zijn. Het diagram is
  niet aangeraakt.
- De andere twee apps (`pong_cleangui_dg`, `pong_cleangui_cg`) houden hun oude namen;
  de conventie geldt voorlopig alleen hier.
- `Widget::enable(bool)` neemt `bIncludeChildren` en geen aan/uit - zie de entry
  hieronder.

## 2026-08-07 — NameInputControl: het invoerveld meldt aan wie de logica heeft

`GameOverDisplayControl` deed meer dan een DisplayControl hoort te doen. Naast de
pagina zat er ook in wat een toetsaanslag betekende, wanneer de invoer compleet was
en waar de ingetypte naam bleef tot iemand erom vroeg.

### Twee keer de verkeerde scheidslijn

Eerst is dat een `HallOfFameInput` geworden: een object dat de invoerwidgets én hun
gedrag bezat, samengevoegd met de taak van de DisplayControl omdat het diens
`PageRoot` deelt. Het werkte, maar er was niets gescheiden - dezelfde mengeling van
beeld en gedrag stond alleen in een ander bestand.

Toen een `NameInputControl` dat via een eigen `IGameOverDisplayListener` te horen
kreeg dat de naam er was. Beter, maar er zat een omweg in: de DisplayControl herkende
ENTER, en had daarna een interface nodig om te vertellen wat ze net had gezien. Twee
plekken die van dezelfde toets moesten weten.

### De scheidslijn die er al lag

Het patroon stond al in de app. `GameControl` is zelf `IButtonListener` en meldt zich
via `gameDisplayControl.addGameOverLabelListener(this)` rechtstreeks aan bij het GAME
OVER-label. De knop is van de DisplayControl; wat een druk erop betekent niet.

`NameInputControl` doet dat nu net zo, met `IInputfieldListener`:

```cpp
IInputfield* addNameInputfieldListener(IInputfieldListener* pInputfieldListener)
{
    inputfield.addInputfieldListener(pInputfieldListener);
    return &inputfield;
}
```

Het veld komt terug, want daar staat de tekst in. Daarmee valt de hele terugweg weg:
`IGameOverDisplayListener`, `getEnteredName()`, `poolEnteredName` en het uitpakken
van de tekst in de tekenlus zijn allemaal verdwenen. De DisplayControl luistert nog
wel mee, maar alleen om te weten dát er iets veranderd is - elke toets behalve ENTER
vraagt met een backbuffer om een hertekening van de hele pagina.

De aanroepkant is één functie geworden:

```cpp
void doQueryName(char* strName, int nofCharsMax)
{
    flagNameEntered.clear();
    gameOverDisplayControl.showNameInput();
    pTask->wait(flagNameEntered);          // UC03 stap 1b
    pNameInputfield->getTextValue(strFieldContent);
    ...
}
```

Een control-object, geen taak: `doQueryName()` draait in de taak van de aanroeper -
zoals `doStartPage`, `doGame` en `doGameOver` - en blokkeert die tot de naam er is,
terwijl de taak van de DisplayControl gewoon doorloopt en het toetsenbord in beeld
schuift. `inputNewKey()` draait weer in de taak die de aanraking opmerkt; vandaar een
Flag en niets anders.

De tekst wordt pas na het wachten uitgelezen en niet in `inputNewKey()`. Dat scheelt
een pool: hij staat al veilig in het veld, en `Inputfield::getTextValue()` zet hem
zelf via een pool over naar de vragende taak.

Verder:

- `GameOverControl::doGameOver()` heeft van de hele naaminvoer nog twee regels over,
  en is `IGameOverDisplayListener` en `flagNameEntered` kwijt. Die hadden daar niets
  te zoeken: het wachten op ENTER lag drie regels van het bijwerken van de top-5.
- `MainControl` meldt de luisteraar niet meer aan. `NameInputControl` doet dat in
  zijn eigen constructor, net als `StartPageControl` en `GameControl` met hun knop.

### Timer heeft nu clear(), net als Flag

In `GameControl::showGameOverLabel()` stond dit na de `waitAny`:

```cpp
pTask->hasFired(flagGameOverLabelPressed);
pTask->hasFired(timerGameOverLabel);
```

Twee aanroepen waarvan de returnwaarde wordt weggegooid, puur om het bijeffect: het
wissen van het event-bit. Dat werkt, maar het staat er als een vraag terwijl het een
opdracht is.

`flagGameOverLabelPressed.clear()` was de duidelijke vorm, en die bestond al -
`timerGameOverLabel.clear()` niet. `Timer` had alleen `stop()`, en die wist het bit
juist *niet*: hij stopt de hardwaretimer, maar een bit dat al gezet was blijft staan.
Precies de val waar de oude opmerking over ging.

Dus `clear()` toegevoegd aan `crt_Timer.h`, met dezelfde body als `Flag::clear()`.
Puur een toevoeging, dus de rest van de bibliotheek en de voorbeelden merken er
niets van. Nu leest de functie symmetrisch: hij opent met een `clear()` van wat er
van voor deze pagina nog kon staan, en sluit af met een `clear()` van wat er net is
gebeurd, plus een `stop()` van de klok zelf.

`stop()` zelf het bit laten wissen was de andere optie, en dan was die val
helemaal weg. Niet gedaan: dat verandert bestaand gedrag, en code die `stop()`
gevolgd door `hasFired()` doet zou er stil op stuklopen.

### En passant: PresentCrate heet nu Crate

`PresentCrate` -> `Crate` en `presentCrate` -> `crate`, overal. Dat raakt drie
bestandsnamen (`pdga_CrateControl.h`, `images/crate-ssh_1x5.png` en de
`_pal8`-header ernaast) en de identifiers `CrateControl`, `imageCrate` en
`arCrateIndices`. Geen botsingen: `CrateDepot` en `crateDepot` heetten al zo.

De png-naam telt hier mee als broncode. `tools/img2rgb565.py` leidt `image=` en
`array=` af uit de bestandsnaam van het plaatje, dus `crate-ssh_1x5.png` levert bij
een volgende hergeneratie vanzelf weer `imageCrate` en `arCrateIndices` op - de
handmatige rename in de header en in `_allImages.h` blijft daarmee staan.

Twee dingen zijn met opzet blijven staan: `UC07_PresentHandling` is de naam van de
use case, en "een present crate" in de kopregel van `pdga_CrateControl.h` is gewoon
wat er in het kistje zit.

### Nog niet aangeraakt

`Widget::enable(bool)` neemt geen "aan of uit" maar `bIncludeChildren`. Op drie
plaatsen in `GameOverDisplayControl::applyPhase` staat het alsof het het eerste is
(`buttonStart.enable(!bName)`), en die regels *enablen* dus altijd. Bij de knop is
dat gemaskeerd doordat `GameOverControl` `flagStartPressed` toch wist voordat hij
erop gaat wachten, en bij het toetsenbord doordat het buiten beeld staat.

## 2026-08-06 — Fakkels, en de derde keer dat de auto-present een half beeld liet zien

Brandende fakkels naast de PLAY-knop en naast PLAY AGAIN: een spritesheet van
tien cellen, 36x72. Onderweg drie keer bijgesteld, en die drie stappen zeggen
meer dan het eindresultaat.

**Eerst was het een driehoekig kuifje dat heen en weer zwiepte.** De fase stuurde
de x-positie van de tongen, en dat leest als een wapperend vlaggetje. Bij vuur
staan de tongen vrijwel rechtop en is het de *hoogte* die pulseert, elk met een
eigen fase. Daarna: een vlamlichaam (een stapel ellipsen die naar boven
versmalt, zodat de vlam een voet heeft) met vijf smalle tongen erboven. Vier
brede tongen liepen in elkaar over tot weer een enkele bundel.

**Losgelaten vlammetjes erbij, en er weer uit.** Als enkele ellips bleven het op
deze maat herkenbare rondjes, en een rondje boven een vuur leest niet als een
vonk maar als een bolletje. Wil je het effect terug, dan is de vorm het probleem
en niet het idee: een vonk hoort een kort verticaal streepje te zijn.

**En de tongen liepen boven de cel uit.** Dat zag je als een vlakke bovenkant. De
fakkelkop zit nu op 58% van de celhoogte in plaats van halverwege - het vuur
heeft meer ruimte nodig dan de steel.

Het afspelen is op twee manieren onvoorspelbaar gemaakt. De wachttijd per stap
wordt geloot tussen 45 en 130 ms, wat een eenmalige `Timer` vroeg die na elke tik
opnieuw wordt gezet in plaats van `start_periodic` - een vaste periode geeft
precies het ritme dat je bij vuur niet wilt. En de cel schuift per stap een of
twee plaatsen op of neer, richting elke keer opnieuw geloot. Vrij loten was te
grof: de tien cellen zijn standen van dezelfde vlam over een cyclus, dus cel 0 en
cel 5 schelen flink en dat leest als flikkeren. Een stap van nul komt niet voor -
twee keer dezelfde cel is een haperende vlam. De ondergrens van 45 ms is er omdat
elke stap een volledige hertekening is, en die kost met de backbuffer ~31 ms.

### De auto-present, deel drie

En toen knipperde het hele startscherm, eens per vijf a tien seconden.

Dezelfde fout als aan het begin van deze sessie, maar in zijn hardnekkigste vorm.
De auto-present van `AsyncDisplay` gaat af zodra de commandoqueue leeg is; treft
hij die leeg aan terwijl er pas een deel van het beeld in staat, dan gaat dat
halve beeld het scherm op. Het eerste commando van elke hertekening is het
schermvullende zwarte achtergrondpaneel, dus een half beeld **is** een zwart
scherm.

De weg erheen, in drie stappen:

| prioriteit t.o.v. asyncDisplay (2) | gedrag |
|---|---|
| 1, eronder | de tekentaak zet de schrijver bij elke opdracht opzij: onophoudelijk knipperen |
| 2, gelijk | FreeRTOS wisselt niet uit zichzelf bij gelijke prioriteit - grotendeels weg |
| 3, erboven | de tekentaak kan de schrijver niet meer onderbreken |

Dat 2 niet genoeg was, kwam pas boven water toen de fakkels de startpagina een
keer of tien per seconde lieten hertekenen. Met time slicing valt er af en toe
een kloktik middenin een reeks commando's, en een kans van enkele procenten per
hertekening wordt dan een flits per vijf a tien seconden. Zolang een pagina
alleen bij een gebeurtenis hertekende, viel dat nooit op.

Uithongeren kan niet op 3: loopt de commandoqueue vol, dan blokkeert de schrijver
daarop en komt de tekentaak alsnog aan de beurt. Alleen de taken die niet tekenen
(Ball, Paddle, Main, Handler) staan nog eronder.

## 2026-08-06 — UC03: hall of fame, naaminvoer, en een PageRoot die er een te klein was

UC03_GameOverHandling is uitgebreid naar wat er nu in het ontwerp staat: haalt de
score een top-5 plek, dan een invoerveld met "TYPE UW NAAM" en het toetsenbord uit
`crt_TouchscreenKeyboard.h`, daarna de lijst met "Play Again" eronder.

De pagina heeft daarvoor twee fasen op **een** PageRoot - NAAM en LIJST - die met
`setDrawable()` worden gewisseld. Niet twee PageRoots: er mag er maar een tegelijk
in beeld staan (zie de opmerking boven `crt_PageRoot.h`), en die eis geldt binnen
een taak net zo goed als ertussen.

Twee dingen weken af van het Inputfield-voorbeeld, allebei door de backbuffer:

- Het toetsenbord schuift met `setLocPos` gevolgd door een volledige
  `page.show(true)`, en niet met `moveTo()`. Met een backbuffer verzet `moveTo()`
  alleen coordinaten van een deel van de boom; zonder hertekening blijft het
  toetsenbord op zijn oude plek in de buffer staan.
- Het veld leegmaken gaat via `keyPressed("DEL")` in een lus. `Inputfield` heeft
  geen `setTextValue`.

### De opslag, en hoeveel er in de partitie past

De top-5 leeft in dezelfde partitie als de touchkalibratie. Het ontwerp zegt
"spiffs"; er draait LITTLEFS op, en aanhaken bij wat er al staat is belangrijker
dan de letterlijke naam - twee bestandssystemen op een partitie gaat niet samen.
Het bestand draagt een versiebyte, want een lijst van een oudere indeling leest
anders als geldige onzin.

De vraag of het past is beantwoord met een meting in plaats van een rekensom,
want LittleFS werkt in blokken van een flash-sector:

```
hallOfFame: bestandssysteem: 102400 bytes totaal, 8192 in gebruik, 94208 vrij;
            de top-5 kost 126 bytes aan gegevens.
```

Die 8192 die al bezet was, is de touchkalibratie: **11 bytes aan gegevens, twee
blokken van 4 KB**. De top-5 komt daar met 126 bytes bij en kost opnieuw ~4 KB.
Van de 100 KB blijft 92 over.

### Drie fouten die geen van drieen crashten

Het GAME OVER-vignet is naar `GameControl` verhuisd (de uitzondering in UC04:
over het spelscherm heen, weg na een tik of drie seconden), en verscheen niet.
Daar zaten er drie achter elkaar in, met aan de buitenkant steeds hetzelfde beeld:

1. **De vlaggen stonden niet in de `waitAny`.** De afhandeling was geschreven,
   maar de tekentaak werd er nooit voor gewekt.
2. **`hasFired()` werd niet aangeroepen.** Dat is in CleanRTOS wat het event-bit
   wist (`clearEventBits`). Omdat het niet uitmaakte wélke van de twee had
   gevuurd, riep ik hem helemaal niet aan - waarna het bit van de timer na de
   eerste keer bleef staan en elke volgende `waitAny` meteen terugkwam. Zichtbaar
   als: de eerste keer wachtte hij drie seconden, daarna nooit meer.
3. **De `PageRoot` van de spelpagina had er een te klein.** 8 vaste widgets + 5
   crates + 5 vuurkorven + 4 scorecijfers + 50 ballen + het vignet is 73; de root
   had 72. `addChildWidget` **crasht daar niet op** - hij logt een regel en geeft
   `false` terug. Het vignet was het laatste in de rij en viel er stilzwijgend
   uit, en omdat de pagina bij het opstarten wordt gebouwd, is die melding allang
   van het scherm als je het merkt.

Het aantal vaste widgets is daarom nu een benoemde constante met de opsomming
ernaast, in plaats van een losse `8` in een template-parameter. En "geen geweigerd
kind bij het opstarten" is sindsdien een vaste controle na het flashen.

## 2026-08-06 — Score, vuurkorven, en een bal die opbrandt in drie stadia

**De score** (UC05 stap 1a) staat tussen de peddelknoppen als vier panels die elk
een cel uit een cijfersheet kiezen. Geen gerenderde tekst: dat scheelt tekentijd,
en vaste celbreedtes houden de score stil - met een proportioneel font verspringt
hij zodra er een 1 in komt. `BallControl` meldt de kaatsing via het veld, alleen
die op de peddel; tegen een wand kaatsen is geen verdienste.

**De vuurkorven** (UC08) kwamen naast de present crates in een eigen `Depot<...,5>`.
Het beslismoment van UC04 staat in een functie: kans op een vuurkorf evenredig met
het aantal ballen, vanaf 40 altijd een. Dat is meteen de rem die het spel nodig
had - elke geopende crate levert er tien bij.

Daar hoorde een terugval bij die het ontwerp niet noemt: **zit de gekozen soort
zijn depot vol, dan de andere.** Zonder dat ging de tik verloren - met een enkele
bal valt de keuze in 39 van de 40 gevallen op een crate, en staan die alle vijf al
in het veld, dan gebeurt er drie seconden lang niets, en daarna weer niet. En dat
verklaarde meteen waarom er in het begin nooit een vuurkorf te zien was.

**Een tweede fout die er vanaf het begin in zat:** niets ruimde de crates en
korven op als een spel eindigde. Ze verdwijnen alleen op eigen voorwaarden - een
crate als hij opengaat, een korf als hij uitbrandt - dus een crate die je niet had
opengespeeld bleef in zijn slot zitten en stond er bij het volgende spel gewoon
weer. `Depot::releaseAll()` erbij, en aangeroepen aan het **eind** van het spel:
de standen bereiken de display pas bij de eerstvolgende beeldstap, en die valt
precies tussen `showGamePage()` en de eerste hertekening in.

**De brandende bal krimpt** terwijl hij opbrandt (UC05 stap 1d): een seconde op
volle grootte, een seconde half, een seconde een kwart. De sheet ging van 6 naar
16 cellen - een gewone bal plus drie stadia van vijf. Het stadium volgt uit de
verstreken tijd, dus een bal die onder de doellijn door valt blijft vanzelf op
volle grootte: die is na ~680 ms weg en komt niet aan het tweede stadium toe. Dat
hoefde nergens apart geregeld te worden.

### Het meetgereedschap staat nu uit

`brandend:`, de beeldtijd met `overgeslagen`, en de spawn-regels zitten achter een
uitgecommentarieerde `//#define PONG_MEASURE`, in dezelfde vorm als
`PONG_FAST_GAME_OVER` en `CRT_ASYNCDISPLAY_COMMAND_TIMING`. Gecontroleerd dat het
project zowel met de meting aan als uit schoon bouwt - een schakelaar die niet
compileert als je hem aanzet is erger dan geen schakelaar.

Die regels kosten geld waar ze het meten: bij 115200 baud is een logregel zo'n
7 ms, en de spawn-regels lopen in de taak van de Handler - een kwart van een
beeld.

## 2026-08-06 — Present crates, en de peddelstand die vijftig keer per beeld op slot ging

UC07_PresentHandling erbij, langs dezelfde weg als de ballen: een
`Depot<PresentCrateControl, 5>`, vijf panels op dezelfde index, en dezelfde
`Handler` die ze aandrijft. De crate spawnt op het beslismoment van UC04 - elke
drie seconden dat er geen bal door de onderlijn gaat - en dat beslismoment is één
functie, `GameControl::spawnObject()`, zodat de keuze tussen mijn en crate later
op precies één plek komt te staan. Voorlopig altijd een crate.

Het aftellen zit in de spritesheet zelf: `presentCrate-ssh_1x5.png`, cel 0 toont
een gaaf kistje met een **5**, elke treffer schuift een cel op en voegt een scheur
toe, cel 4 toont een **1**. De code kiest alleen de cel met
`NofCrateHits - nofHitsLeft`. Opaak weggeschreven (geen alfakanaal): het kistje
dekt zijn hele cel, dus blitten is een `memcpy` per beeldregel in plaats van een
per zichtbaar stuk.

Twee dingen die het ontwerp niet zegt en die toch moesten:

- **De bal kaatst tegen een crate.** Zonder kaatsing is de teller zinloos: een bal
  die er dwars doorheen vliegt overlapt vijf beelden achter elkaar en sloopt het
  kistje in tweehonderd milliseconde. Hij wordt teruggeduwd langs de as waar hij
  het minst diep zit - altijd verticaal kaatsen laat een bal die van opzij komt er
  dwars doorheen schieten.
- **`IBallControlListener` is vervallen.** Het einde van het spel hangt sinds UC07
  niet meer aan één bal maar aan "geen ballen meer in het spel" (UC04 stap 4), en
  alleen wie het depot afloopt weet dat. Nu `IGameObjectsListener::allBallsGone()`,
  op de overgang naar nul - want vóór de eerste spawn staat die teller ook op nul.
  Zou `GameControl` zelf `getNofInUse()` pollen, dan kon het een verouderde waarde
  lezen en voorgoed blijven wachten.

### Vijftig ballen, en een slot dat vijftig keer per beeld omging

Met vier crates die vlak na elkaar opengingen liep het depot vol tot 50 ballen.
Alles bleef kloppen - `0 overgeslagen` in élke seconde, geen assert, beide depots
meldden hun uitputting netjes in plaats van te stoppen - maar de Handler ging over
zijn periode heen:

```
I (178595) Probleem: Handler Delay too long in us:54628
I (179205) Probleem: Handler Delay too long in us:49440
```

De oorzaak zat in `BallControl::step()`, dat per bal per beeld
`PaddleControl::getPaddlePos()` aanriep. Dat gaat door een `Pool`, en die neemt
een `SimpleMutex`. Met vijftig ballen zijn dat vijftig slotoperaties per beeld op
een slot dat óók door `PaddleControl` wordt genomen - en die draait op prioriteit
1, dus ónder de Handler. Houdt die het slot vast op het moment dat de Handler het
wil, dan wacht de Handler op een taak die zelf niet aan de beurt komt.

`GameObjectsControl` leest de peddelstand nu één keer per beeld en zet hem in een
`int` die de ballen via een `const int*` lezen - in dezelfde taak, dus er valt
niets te vergrendelen. Alle ballen van een beeld zien bovendien gegarandeerd
dezelfde peddel, wat sowieso hoort.

Gemeten na de ingreep, bij dezelfde belasting (41 ballen): **nul overruns in
negentig seconden**, tegen twee ervoor.

Wat blijft staan is een wiebel aan de displaykant: in vier van de tweeëntwintig
gemeten seconden `beeldtijd 11..66 ms` in plaats van 37..40 - één beeld dat ~26 ms
te laat komt en één dat er meteen achteraan zit. Het gemiddelde blijft 26 fps en
`overgeslagen` blijft 0, dus er gaat geen spelstand verloren; alleen de cadans van
het tekenen loopt onder druk niet gelijk. Vermoeden, niet gemeten: met 41 ballen
gaan er zo'n zestig commando's per hertekening de queue in, `AsyncDisplay` doet er
tien per ronde en pollt er de touch tussendoor, en de auto-present gaat pas af als
de queue leeg is.

## 2026-08-06 — Ballen uit een Depot: vijftig objecten, één taak

`BallControl` is geen `Task` meer. Er kunnen er tot vijftig tegelijk in de lucht
zijn (UC07 spawnt er tien ineens), en vijftig taken van 5000 bytes stack is 250
KB - meer DRAM dan dit bord heeft, zeker met de backbuffer van 76.800 bytes erbij.
Ze liggen nu geprealloceerd in een `Depot<BallControl, 50>` en doen per beeld een
stap in `update()`, aangedreven door één `crt::Handler`. Dat is precies waar
`Handler` voor gemaakt is; zijn eigen kopregel zegt het al.

`Depot<T,N>` is nieuw, en staat in de app (`pdga_Depot.h`, `pdga_IDepot.h`) en niet
in CleanRTOS. `obtain()` geeft een **index** terug, niet een referentie: die index
wijst behalve het object ook het panel aan dat ernaast ligt. Bij uitputting
`InvalidIndex` en een melding, bewust geen assert - een spel dat tien objecten
ineens wil spawnen komt daar vanzelf een keer aan.

Het belangrijkste ontwerpdetail: **`arInUse` wordt nooit samengeperst.** Daardoor
mag een bal zichzelf midden in zijn eigen `update()` vrijgeven - de index-scan
loopt gewoon door. Dat is de reden dat "de bal geeft zichzelf terug zodra hij het
scherm uit is" veilig is, en dat er een bool-array staat en geen lijst van actieve
indices.

Twee dingen die de vorm bepaalden:

- **Een `Flag` per bal kan niet.** `crt_Task.h` doet `assert(nofWaitables < 24)`,
  dus een taak bezit er hooguit vierentwintig. Alle standen gaan daarom in één
  array met één vlag erachter.
- **`Pool::write(T item)` neemt op waarde aan.** Zouden vijftig ballen elk zelf
  schrijven, dan is dat vijftig keer ~800 bytes kopiëren per beeld. Vandaar dat de
  ballen nergens naartoe schrijven maar worden uitgelezen, in één publicatiestap
  per beeld.

Dat publiceren doet `GameObjectsControl`, en dat is het enige dat zich bij de
Handler aanmeldt. Het depot ís ook een `IHandlerListener` en had er dus
rechtstreeks bij gekund, met een tweede listener erachter voor het publiceren -
maar dan bepaalt de registratievolgorde bij de Handler of het beeld klopt. Nu
staat die volgorde in de functie zelf.

### Een fout in crt_Handler.h die er waarschijnlijk al lang lag

Het project bouwde niet:

```
crt_Handler.h:47: error: member 'crt::Handler<4>::infiniteBatchSizeUs' is used uninitialized
```

De constructor met vier parameters delegeert naar die met vijf en geeft
`infiniteBatchSizeUs` als argument mee - maar dat was een **niet-statisch** lid,
dat op dat moment nog niet bestaat. `batchSizeUs` kreeg daarmee een willekeurige
waarde, waardoor er stilzwijgend `taskYIELD()`s tussen de updates konden vallen in
plaats van "geen limiet". Nu `static constexpr`, dus hij bestaat los van het
object. Dit is de eerste keer dat `Handler` in dit project wordt gebruikt.

Twee dingen in datzelfde bestand zijn **niet** aangeraakt, omdat ze elke
Handler-gebruiker raken:

- De klok loopt ~2,5% te snel. `vTaskDelay((periodUs - diffAll) / 1000)` kapt af
  naar hele milliseconden, dus hij slaapt elke ronde tot 1 ms te kort. Gemeten:
  38-39 ms per beeld op een periode van 40, en 26 fps waar 25 bedoeld is.
- De overrun-melding gaat bij elke boot één keer loos af
  (`Handler Delay too long in us:500419`): `main()` zet zijn referentietijd vóór de
  `vTaskDelay(500)` waarmee hij zijn luisteraars de kans geeft zich te melden, en
  telt die wachttijd mee als overschrijding van de eerste ronde. Een detector die
  bij elke boot loos afgaat, leer je negeren.

## 2026-08-06 — De brandende bal, en twee sprongen die geen sprongen mochten zijn

UC05_BallHandling stap 1b: een bal die de onderlijn passeert vat vlam, vliegt
brandend door tot buiten het scherm, en pas dán is het game over. De bal komt uit
een spritesheet van zes cellen van 28x28 - cel 0 de gewone bal, cel 1..5 de
brandcyclus. De cel is ruimer dan de bal (12 px) zodat de vlammen ergens heen
kunnen zonder dat de botsingsmaat verandert; `BallSizePix` bleef 12. De cel-maat
wordt uit `imageBall.width / nofCols` afgeleid, zodat er geen tweede plek is waar
die 28 kan verouderen.

Gemeten: 17 stappen, 68 pixels, ~3,5 vlamcycli. De ballen worden als laatste aan
de pagina toegevoegd en dus bovenop getekend - anders verdween de brandende bal
vlak onder de doellijn achter een peddelknop, en juist het doorvliegen was het
punt.

### Twee sprongen, allebei door hetzelfde patroon

"Het lijkt wel of de bal wat sneller gaat, als hij eenmaal in brand staat." Dat
klopte, en het stond in de meting: `beeldtijd 1..44 ms` en `3..44 ms`, terwijl
alle andere seconden strak op 42-44 zaten. Bij het passeren van de doellijn werd
de positie **ongestapt** doorgestuurd, waarna de eerstvolgende ronde op hetzelfde
moment de gestapte stuurde. Het scherm tekende twee beelden binnen enkele
milliseconden en de bal legde zijn hele stap af in een oogwenk. Dezelfde fout
bleek bij het starten van elk spel te zitten. Beide melding weggehaald; de vlammen
verschijnen nu een beeld later, en dat klopt ook beter - hij vat vlam terwijl hij
de lijn passeert.

### vTaskDelay is een pauze, geen periode

De brandmeting gaf 91 px/s waar 100 verwacht werd: 43,6 ms per stap in plaats van
40. `vTaskDelay(40)` pauzeert vanaf **nu**, dus de looptijd van de lus komt er
bovenop - en hoeveel dat is hangt af van hoe druk de tekentaak het heeft.
`xTaskDelayUntil` rekent vanaf het vorige wekmoment en haalt dat eruit. Meteen
daarna gemeten, drie keer identiek: **17 stappen, 68 px in 680 ms = 100 px/s**.

Eén valkuil zit erin: na de game-over-pagina staat het wekmoment seconden in het
verleden, en dan haalt `xTaskDelayUntil` die achterstand in met een reeks stappen
zonder pauze - precies de schok die er net uit was. De klok wordt daarom bij elke
nieuwe bal opnieuw gezet.

**Een gevolgtrekking die niet klopte, voor de duidelijkheid:** uit de eerdere
`beeldtijd 41..45 ms` was afgeleid dat het tékenen ~43 ms nodig had, en daar hing
een voorstel aan om `present()` alleen het veranderde gebied te laten sturen. Dat
was onjuist. Die extra milliseconden waren volledig de drift van `vTaskDelay`; na
de vaste klok staat de beeldtijd op 39..40 ms. Het tekenen zat altijd al binnen de
40 ms, en de aanleiding voor die ingreep verviel daarmee.

## 2026-08-06 — pong_cleangui_dg_adv: alles uit 8bpp paletted bitmaps

`pong_cleangui_dg` is gekopieerd naar `pong_cleangui_dg_adv`, met prefix `pdga_`
en namespace `pong_cleangui_dg_adv` - nodig, want alle drie de varianten staan
tegelijk in het includepad. Daarna is de app omgezet naar
`TFT_eSPI_DisplayAdapter_PAL8`: een schermvullende backbuffer van 320x240 bij 8
bits per pixel (76.800 bytes), net als het MultiPanel-voorbeeld. Hij past, en DMA
komt op.

Acht objecten komen nu uit een PAL8-plaatje: de drie wanden, de doellijn, de
peddel, de bal, de PLAY-knop en "GAME OVER" - later gevolgd door de twee
peddelknoppen. Samen ~42 KB indices in flash. De bronnen staan in `images/` en
worden getekend door `images/make_pong_images.py`, zodat ze bij te stellen zijn
zonder tekenprogramma; omzetten gaat met `tools/img2rgb565.py --pal8`.

`images/_pal8/_userPalette.h` is met de hand geschreven: de kleuren die de code
zélf nog noemt (de zwarte achtergrond, de rode peddelknoppen, hun witte tekst)
liggen daarmee op een vaste index, zodat `indexForColour()` ze op afstand 0 vindt
in plaats van "iets dat de median-cut toevallig heeft opgeleverd".

Twee dingen volgden uit de adapter en niet uit smaak. Afgeronde hoeken worden
vierkant - een `TFT_eSprite` heeft er geen primitieve voor - dus alles wat rond
moet zijn is nu een plaatje, inclusief het kader achter "GAME OVER" en de
schuine kant van de knoppen. En gedeeltelijk hertekenen is er niet meer bij:
`present()` stuurt altijd het hele scherm (~31 ms), dus per beeld de hele boom
opnieuw opbouwen is goedkoper dan het slim proberen te doen. De DisplayControls
vragen `display.isBuffered()` en kiezen zelf; met de RGB565-typedef terug werkt de
oude `moveTo()`-route nog.

### Het hele scherm knipperde, en dat was een taakprioriteit

Ook zonder iets aan te raken flikkerde het beeld, ongeveer drie keer per seconde.
MultiPanel doet dat niet.

`AsyncDisplay` presenteert uit zichzelf zodra zijn commandoqueue leeg is en er iets
getekend is - die auto-present neemt "queue leeg" als "beeld af". De drie
DisplayControls van pong draaien op prioriteit 1, dus **onder** de tekentaak op 2.
Een schrijver die daar zit, wordt bij zijn ingestuurde opdrachten opzij gezet, en
de tekentaak treft de queue dan leeg aan terwijl er pas een deel van het beeld in
de buffer staat. Het eerste commando van elk pong-beeld is het schermvullende
zwarte achtergrondpaneel - dus zo'n half beeld ís een zwart scherm.

Alle voorbeelden in `libs/CleanGUI_for_TFT_eSPI/examples` draaien hun tekenende
taak om deze reden op 2. Na de drie DisplayControls naar 2 te hebben gezet, was
het knipperen weg. Vóór de omzetting viel het niet op: bij de RGB565-adapter is
`present()` leeg en raakte `moveTo()` alleen kleine rechthoekjes aan.

## 2026-08-05 — show/hide per pagina, in beide varianten gelijk

De paginafuncties heetten eerst `selectXxxPage()`, daarna kort
`selectXxxPage()`/`deselectXxxPage()`, en nu `showXxxPage()`/`hideXxxPage()`.
Die laatste ronde had een reden: `select` belooft exclusiviteit - het tonen van
de ene pagina zet de andere uit - en dat maakt geen van beide varianten waar. In
`_dg` kan het niet eens: elke DisplayControl kent alleen zijn eigen pagina. De
exclusiviteit komt van de aanroeper, die zijn pagina toont aan het begin van zijn
`do..()` en verbergt aan het eind, met `MainControl` die die functies strikt na
elkaar aanroept.

Wie dat paar niet netjes houdt, maakt een stille fout: een vergeten hide laat de
pagina "aan" staan. Je ziet dat niet - het verbergen tekent toch niets - maar zijn
knoppen blijven wel gevoelig, en dan bedien je de peddel op de game-over-pagina.

Datzelfde patroon is nu ook op `_cg` toegepast, en daar was het meer dan een
hernoeming:

- `IDisplayControl` heeft zes functies in plaats van drie: een paar per pagina.
- De oude `selectXxxPage()` wiste zelf de vlaggen van de andere twee. Dat kon weg.
- **De `State`-enum en de hele `switch(state)` zijn verdwenen.** Die bestonden
  alleen om bij te houden welke pagina de huidige was. Nu elke pagina zijn eigen
  paar heeft, is de hoofdlus een platte lijst gebeurtenissen - structureel
  hetzelfde als de drie losse DisplayControls van `_dg`. Dat maakt het vergelijken
  van de twee varianten eerlijker: het verschil gaat nu over waar de graphics
  wonen, niet over hoe de toestand wordt bijgehouden.
- De private helpers heten in beide varianten `showPage()` / `hidePage()`. In
  `_cg` nemen ze een `IWidget&`, want de drie pagina's zijn `Panel<8>`, `Panel<2>`
  en `Panel<1>`.
- De `setDrawable(false)`-hide uit `_dg` is overgenomen. In `_cg` is die **niet**
  nodig om races te vermijden - er tekent maar een taak - maar hij haalt de zwarte
  flits tussen twee pagina's weg en houdt de varianten vergelijkbaar. Wel erft
  `_cg` daarmee dezelfde eis: elke pagina moet met een schermvullend paneel
  beginnen.
- `showStartPage()` in `main()` is weg; `StartPageControl` vraagt de pagina nu zelf
  op, net als in `_dg`.

Beide varianten bouwen schoon en `_cg` boot schoon op het bord. **Voorbij het
opstartscherm is deze `_cg`-versie nog niet uitgeoefend**: een meting van drie
minuten leverde geen enkele `pong: XX fps`-regel op, dus er is niet gespeeld. Het
eerste wat nagelopen moet worden is of de startpagina bij het opstarten nog
verschijnt - dat is de plek waar de weggehaalde `showStartPage()` uit `main()`
zich moet bewijzen.

## 2026-08-05 — pong_cleangui_dg: een DisplayControl en een PageRoot per pagina

De `_dg`-variant is opnieuw opgezet vanaf een kopie van `_cg` (zie README.txt
daar): elke pagina krijgt zijn eigen DisplayControl met een eigen PageRoot en een
eigen taak. `pdg_DisplayControl.h` is daarmee opgeheven en verdeeld over
`pdg_StartPageDisplayControl.h`, `pdg_GameDisplayControl.h` en
`pdg_GameOverDisplayControl.h`. In plaats van `selectStartPage()`,
`selectGamePage()` en `selectGameOverPage()` op een gedeelde interface luistert
elke DisplayControl nu naar zijn eigen `showXxxPage()` / `hideXxxPage()`.

De spelmaten (`FrameIntervalMs`, `WallThicknessPix`, ...) en `TwoNumbers` staan nu
in `pdg_GameDisplayControl.h`: ze beschrijven het speelveld, en `BallControl` en
`PaddleControl` includeren die header toch al voor `IGameDisplayControl`.

### Wat deze opzet makkelijker maakt

De `show(true)`-val uit de cg-variant bestaat hier niet. Daar hingen drie pagina's
onder een gedeelde root, en dan is `pageRoot.show(true)` fout (het toont ze alle
drie) terwijl `show(false)` nodig is om de root zelf zichtbaar te krijgen - iets
wat makkelijk te vergeten is, en precies wat er misging. Hier draagt elke root
maar een pagina, dus is `page.show(true)` gewoon het juiste, en de root wordt
vanzelf zichtbaar. Een hele klasse fouten valt weg.

### Wat deze opzet moeilijker maakt: drie taken die tekenen

Bij een paginawissel verbergt de ene taak zijn pagina terwijl de andere de zijne
toont. Beide schrijven in dezelfde commandobuffer van `AsyncDisplay`, en welke er
als laatste uitkomt ligt niet vast - dan wist het ene beeld het andere. De
originele `_dg` loste dat op met `vTaskDelay(200); // wacht op evt parallel
afrenderen van scene`; dat is een gok, geen garantie.

Hier is het opgelost door het verbergen *niets te laten tekenen*:

```c
page.setDrawable(false, true);   // Panel::hide() slaat zijn wisvlak nu over
page.hide(true);
page.disable(true);
```

`Panel::hide()` tekent zijn wisvlak alleen als `isDrawable()`, dus zo kost het
verbergen geen enkel commando en valt er niets meer te racen: wat overblijft zijn
`_bVisible` en `_bEnabled`, en die zijn per pagina apart. Bij het tonen gaat
`setDrawable(true, true)` er weer vooraf.

Dat mag omdat elke pagina begint met een schermvullend `backgroundPanel`: wie in
beeld komt wist wat de vorige achterliet. **Die eis is nu dragend** - een pagina
die het scherm niet volledig dekt laat resten van zijn voorganger staan. Als
bijvangst is de wissel ook mooier: geen zwarte flits tussen twee pagina's in.

De tweede helft van de afspraak zit in de control-objecten: elke `do..()` toont
zijn pagina aan het begin en verbergt hem aan het eind, en `MainControl` roept ze
strikt na elkaar aan. Daarmee staat er nooit meer dan een PageRoot in beeld - de
eis die boven `crt_PageRoot.h` staat.

`main/CMakeLists.txt` bevatte nog een waarschuwing dat de twee pong-apps dezelfde
headernamen dragen. Dat geldt sinds de `pcg_`/`pdg_`-prefixen alleen nog voor
`apps/pong/pong`; bijgewerkt.

### Op hardware beproefd, inclusief de paginawissels

`idf.py build` is groen en het bord boot schoon: geen assert, paniek of watchdog,
en geen van de klachten die CleanGUI zelf uitstuurt. Met drie extra taken erbij
zegt dat al iets - de drie PageRoots met hun widgetbomen zijn opgebouwd zonder
ruimtegebrek en de vier touchlisteners passen.

Daarna een volle ronde gespeeld: startpagina -> spel -> game over -> startpagina
-> tweede spel, dus drie paginawissels op rij. Uit de seriële meting:

```
I (5230)  pong: 1 fps      <- spel 1 begint
I (6255)  pong: 26 fps
I (10260) pong: 25 fps     <- spel 1 loopt ~5 s
I (13864) pong: 10 fps     <- restant spel 1 + eerste frame spel 2
I (14864) pong: 25 fps
I (18943) pong: 25 fps     <- spel 2 weer op tempo
```

Dat het tweede spel opnieuw 25 fps haalt is het stuk dat vooraf niet vaststond:
bal en peddel tekenen dus nog na een volledige ronde van `disable`/`enable` op
alle drie de pagina's, inclusief het `bPressed`-valletje in
`TouchscreenButton::enable()`.

Visueel nagelopen door Marius: geen artefacten tussen de pagina's. Daarmee is ook
de aanname bevestigd waar de niet-tekenende `hide` op leunt - elke pagina dekt
zijn voorganger volledig.

## 2026-08-05 — pong_cleangui_cg: alle GUI in DisplayControl, de rest weer aan de praat

De refactor die alle displayafhankelijke zaken naar `DisplayControl` verhuist was
halverwege blijven steken. Wat er nog aan mankeerde, per soort.

**Halve zinnen.** `class MainControl : public Task` stond op de plek van de
constructornaam, `void doGame` miste zijn haakjes, `displayControl.add` was een
regel die ophield, en de initialisatielijst van `GameControl` eindigde op een
komma. `GameOverControl` initialiseerde `pStartPageStartButton` terwijl zijn
member `pGameOverPageStartButton` heet.

**Namen die niet meer bestonden.** In `DisplayControl::main()` heetten de panels
nog `pageGame` en `startPanel`, de toestand `STATE_GAMEOVER` naast
`STATE_GAME_OVER`, en `initBallAndPaddlePosition()` naast
`initBallAndPanelPosition()`. `enabled(true)` moest `enable(true)` zijn. En de
drie knoppen kregen hun eigenschappen via `setPanelProps()`, die de tekst- en
fontparameters helemaal niet kent - dat is `setTouchScreenButtonProps()`.

**De knoppen deden niets.** Nergens werd `display.addTouchListener()` aangeroepen.
In de `_dg`-versie deden `PaddleControl` en `GameControl` dat elk voor hun eigen
knoppen; nu wonen alle vier de knoppen in `DisplayControl` en meldt die ze in zijn
constructor aan. Een uitgeschakelde knop negeert een aanraking zelf, dus ze mogen
alle vier meteen mee.

**De peddelknoppen hadden geen plek.** `PaddleControl` verwees nog naar zijn
`pagePaddles`, die met de verhuizing verdwenen was. De knoppen zitten nu als kind
in `gamePanel`. Daarvoor moest `gamePanel` het *hele* scherm beslaan in plaats van
alleen het speelveld: het speelveld is de bovenste driekwart, de knoppen zitten in
de onderste kwart, en de promillagecoördinaten van die knoppen zijn op het scherm
gebaseerd. Dat heeft een tweede voordeel: `moveTo()` herstelt de vrijgekomen
strook via de boom, en met een fullscreen `gamePanel` lukt dat ook onder de
doellijn - waar de bal aan het eind van een spel verdwijnt.
`PaddleControl` roept nu `displayControl.showPaddle()/hidePaddle()` aan en tekent
zelf niets meer. Dat is de hele bedoeling van deze variant.

**Het spel begon nooit.** `doGame()` selecteerde de gamepagina en ging wachten op
een gemiste bal, maar riep `ballControl.startBall()` en `paddleControl.enable()`
niet aan. Toegevoegd, plus de bijbehorende `paddleControl.disable()` als het spel
uit is.

**Pagina's die over elkaar heen bleven staan.** Bij het tonen van een nieuwe
pagina werd de oude niet verborgen. Er zijn nu vier hulpfuncties
(`showGamePage`/`hideGamePage` enzovoort) en elke overgang gebruikt ze.

**Vlaggen die in de verkeerde toestand bleven hangen.** Dit is de venijnigste:
`waitAny()` wekt alleen op de vlaggen die erin staan, dus een `flagSelectStartPage`
die binnenkomt terwijl `DisplayControl` in `STATE_GAME_OVER` alleen op
`flagSelectGamePage` wacht, blijft eeuwig staan - en `MainControl` wacht dan op een
startpagina die nooit verschijnt. Alle drie de paginavlaggen staan nu in de
`waitAny()` van elke toestand. Om dezelfde reden wist `hideGamePage()` de twee
peddelvlaggen: `paddleControl.disable()` valt samen met het einde van het spel, en
die `flagHidePaddle` zou anders in het volgende spel de peddel meteen weer laten
verdwijnen. En `doStartPage()`/`doGameOver()` wissen `flagStartPressed` voor ze
gaan wachten: de twee startknoppen liggen op precies dezelfde plek, dus de druk
waarmee je het vorige scherm wegklikte zou het volgende meteen weer wegklikken.

**Op hardware gedraaid.** `idf.py build` is groen zonder waarschuwingen uit de
`pcg_`-bestanden, en na flashen boot het bord schoon: kalibratie uit spiffs,
geen assert, geen paniek, geen watchdog. Uit de seriële meting van 60 seconden:
na een aanraking van de startknop komt de fps-teller van `DisplayControl` op
gang en houdt hij `25 fps` vast - precies de `FrameIntervalMs = 40` die
`BallControl` aanhoudt. Daarmee staat vast dat de aanmelding als touchlistener
werkt, dat `doGame()` de bal daadwerkelijk start, en dat de gamepagina zijn
balupdates op tempo verwerkt.

Maar de meting loog: op het bord stond het beeld stil terwijl de teller vrolijk
25 fps rapporteerde. Zie het stuk hieronder - dat is meteen de les.

### De root die nooit zichtbaar werd

Het beeld verscheen wel maar bewoog niet, peddel noch bal. Toch liep de
fps-teller op 25. Dat verschil wijst precies de plek aan: die teller staat naast
de `moveTo()` in dezelfde tak en telt *verwerkte events*, geen getekende pixels.
Het hele signaalpad - `BallControl` -> `Pool` -> `Flag` -> `waitAny` ->
`hasFired` -> `moveTo` - werkte dus perfect. Alleen de laatste stap deed niets.

De oorzaak: `PageRoot` had nooit een `show()` gehad, en `_bVisible` begint op
`false` (`crt_Widget.h:50`). Er lopen namelijk twee verschillende paden naar het
scherm:

- *voor het eerst tekenen* gaat via `Panel::show()` -> `showPanel()`, en dat
  roept regelrecht `getDisplay()->fillSmoothRoundRect()` aan. Het kijkt niet naar
  `_bVisible` en niet naar de root. Vandaar dat het beeld gewoon verscheen.
- *opnieuw tekenen na een verplaatsing* gaat via `moveTo()` -> `repaintRegion()`
  -> `getRoot()->redrawRegion()`. En `redrawRegion()` begint met
  `if (!_bVisible) { return; }` (`crt_Widget.h:357`).

Op een onzichtbare root keert die dus meteen terug, en dat is de hele boom in
één keer. Niet alleen werd de bal niet op zijn nieuwe plek getekend - ook de
strook die hij verliet werd niet hersteld, want die gaat langs dezelfde weg.

`moveTo()` komt bij die root omdat `ballPanel` een `cornerRadius` heeft:
`Panel::coversOwnRect()` geeft dan false (`crt_Panel.h:182`), en een widget die
zijn eigen rect niet volledig dekt moet de boom eronder laten meetekenen, anders
schemert het vorige beeld door de ronde hoeken. `paddlePanel` heeft een radius
van `PaddleHeightPix/3` en liep tegen exact dezelfde muur.

De fix is `pageRoot.show(false)` direct na het aanmaken. **`show(true)` zou hier
fout zijn**: `Widget::show()` daalt dan af naar alle kinderen en tekent de drie
pagina's over elkaar heen - en zet hun `_bVisible` op true, waarna elke
`redrawRegion` de game-over-pagina en de startpagina over het speelveld blijft
tekenen (ze staan later in de kinderlijst dan `gamePanel`). De root is een kale
`Widget` en tekent zelf niets, dus `show(false)` zet alleen de vlag om.

Waarom dit in `_dg` niet bestond: daar *is* de PageRoot de pagina, dus
`pageGame.show(true)` zette de root vanzelf zichtbaar. Het cg-ontwerp hangt drie
pagina's ónder één root. Bij die verhuizing is de `show()` op paginaniveau blijven
staan en heeft niemand de root nog aangezet. Dezelfde `_bVisible`-regel is
tegelijk wat wél gewenst is: hij zorgt dat een verborgen `startPagePanel` niet
meetekent als de bal over de gamepagina beweegt. De root kent daarop geen
uitzondering - en dat is precies waar dit misging.

Eén ding met opzet gelaten zoals het stond: `MainControl` gaat van
`STATE_GAME_OVER` naar `STATE_START_PAGE` en niet rechtstreeks naar
`STATE_GAME_RUNNING`. Na een spel zijn er dus twee klikken nodig om opnieuw te
beginnen, terwijl op de game-over-knop "Click here to Start" staat. Dat is een
ontwerpkeuze, geen fout, dus onaangeroerd.

Los daarvan: `apps/pong_cleangui_dg` includeerde nog de oude `crt_`-namen van
bestanden die in dezelfde commit `pdg_` werden. Die staan in het includepad maar
worden niet gebouwd, dus het viel niet op. Rechtgezet.

## 2026-08-03 — Bouwfouten in pong_cleangui_cg: restanten van een omgedraaide koppeling

De nieuwe `_cg`-variant draait de koppeling tussen bal en peddel om. Vroeger
duwde `PaddleControl` de positie naar de bal met
`ballControl.updatePaddlePosition(...)`; nu haalt `BallControl` hem zelf op met
`paddleControl.getPaddlePos()`, met `poolPaddlePos` eronder.

Van de oude richting stonden er drie dingen nog in `pcg_PaddleControl.h`, en die
gaven samen zes compilerfouten:

- `#include "pcg_BallControl.h"`, terwijl `pcg_BallControl.h` nu andersom
  includeert. Met `#pragma once` betekent dat: wie als eerste binnenkomt ziet de
  ander nog niet gedeclareerd staan. Vandaar `'PaddleControl' does not name a
  type` in het ene bestand en `does not have any field named 'ballControl'` in
  het andere - twee symptomen van dezelfde kring.
- `ballControl(ballControl),` in de initialisatielijst, zonder member en zonder
  constructorparameter van die naam.
- `ballControl.updatePaddlePosition(paddlePosition);` in `movePaddle()`. Die
  functie bestaat in de `_cg`-`BallControl` niet meer; de `poolPaddlePos.write()`
  op de regel erboven doet het werk.

Alle drie verwijderd. De include hoort maar één kant op te lopen: BallControl
kent PaddleControl, niet andersom.

Daarna bleef er een linkfout over: `undefined reference to
pong_cleangui_cg::logger`. De vier `pcg_`-headers hadden de regel
`extern ILogger& logger;` meeverhuisd naar de nieuwe namespace, en daarmee
kondigden ze een object aan dat niemand definieert. De enige logger is
`crt::logger` uit `main/main.cpp:24`. In de oorspronkelijke pong stonden deze
bestanden zelf in namespace `crt`, dus daar klopte die regel nog. Nu niet meer:
de extern-declaraties zijn weg, en het `using namespace crt;` dat boven elk van
die bestanden al stond zorgt dat `logger` gewoon bij `crt::logger` uitkomt.

`apps/pong_cleangui_dg` staat wel in de include-paden van `main/CMakeLists.txt`,
maar `main.cpp` includeert alleen `Pong_CleanGUI_cg.ino` - die variant is dus
niet meegecompileerd en niet gecontroleerd.

## 2026-08-03 — Code-navigatie in VS Code over WSL

Geen codewijziging; alleen gereedschap. In het Remote-WSL-venster gaf
rechtsklikken op een functie geen "Go to Declaration".

### Waar het aan lag
Bij Remote-WSL draaien taalextensies aan de Linux-kant, in
`~/.vscode-server/extensions`. Daar stond alleen `github.copilot-chat` - geen
enkele C/C++-taalserver. Wat er in de Windows-installatie staat telt niet mee.
VS Code behandelde `.h`/`.cpp` dus als platte tekst.

### Wat er is gedaan
- `ms-vscode.cpptools` (1.32.2, linux-x64) en `espressif.esp-idf-extension`
  (2.1.0) geinstalleerd aan de WSL-kant, met
  `~/.vscode-server/bin/<hash>/bin/code-server --install-extension ...`.
  De remote-cli wrapper werkt hier niet: die eist `VSCODE_IPC_HOOK_CLI` en
  is dus alleen bruikbaar vanuit de integrated terminal.
- `.vscode/c_cpp_properties.json` (gitignored) wijst naar het al bestaande
  `build/compile_commands.json` - 1648 vertaaleenheden, ververst door
  `idf.py build`. Standaarden afgelezen uit de build zelf: `gnu17` en
  `gnu++2b`.
- Daarbij `includePath`/`browse` over `apps/`, `libs/`, `main/`,
  `components/`. Nodig omdat deze codebase grotendeels header-only is: van
  dit project staan alleen `main/main.cpp` en een paar library-`.cpp`'s in de
  compile database, dus `crt_*.h` is zelf geen vertaaleenheid.

### Wat er anders liep dan verwacht
ESP-IDF-extensie 2.1.0 heeft de wizard "Configure ESP-IDF Extension" met
"Use Existing Setup" niet meer, en ook de instellingen `idf.espIdfPath` /
`idf.toolsPath` / `idf.pythonBinPath` bestaan niet meer. In plaats daarvan
zoekt hij installaties via de ESP-IDF Installation Manager
(`~/.espressif/tools/eim_idf.json`). Deze IDF is klassiek geinstalleerd met
`install.sh` en heeft dat bestand niet.

De extensie kijkt daarnaast nog wel naar het oude `<toolsPath>/esp_idf.json`,
en `$HOME/.espressif` staat vast in de zoeklijst. Daar is dat bestand met de
hand neergezet: v5.4.3, `/home/marius/esp54/esp-idf`, python uit
`idf5.4_py3.10_env`. Zo blijft de klassieke installatie bruikbaar zonder EIM
erbij te halen.

## 2026-08-02 — De dode startknop: een belletje dat niets meer dekte

Symptoom in pong_cleangui: bij het game-over-scherm was ongeveer een op de drie
keer de herstartknop voorgoed dood. Het scherm klopte - "GAME OVER" en de knop
stonden er netjes - maar er kwam geen enkele reactie meer, en alleen de
resetknop hielp. Geen paniek, geen watchdog, geen enkele regel op de seriele
poort. Vandaar de aanname dat er "stil iets gecrasht" was.

### Waar het aan lag
`Queue::write()` doet twee dingen na elkaar die als een geheel bedoeld zijn:

```cpp
xQueueSend(qh, &v, writeDelay);      // het item staat er nu in
pTask->setEventBits(getBitMask());   // ... en nu pas gaat de bel
```

Wordt de schrijver daar precies tussenin weggedrukt, en leegt de lezer de queue
intussen helemaal, dan komt die `setEventBits` alsnog - op een lege queue. Het
event-bit is daarmee een belletje dat niets meer dekt. `Task::hasFired()` wist
queue-bits met opzet niet (alleen een `read()` mag dat doen), dus het blijft
staan ook.

`AsyncDisplay::main()` geloofde dat bit en riep `Queue::read()` aan, en die
blokkeert met `portMAX_DELAY` op een queue waar niets in zit. Daarmee stond de
tekentaak stil - en `pollTouch()` draait in diezelfde taak. Dat is precies het
beeld: het paneel houdt vast wat er stond (de directe RGB565-adapter tekent
rechtstreeks in GRAM), en er wordt nooit meer een aanraking uitgelezen.

Twee schrijvers zijn genoeg, en die zijn er bij game over: `GameControl`,
`PaddleControl` en `SceneDisplayControl` tekenen daar alle drie binnen dezelfde
200 ms. Tijdens het spelen tekent alleen `SceneDisplayControl`, en dan kan het
niet gebeuren. Dat verklaart waarom het uitgerekend bij die overgang zat.

### Hoe het gemeten is - en wat de eerste poging kapotmaakte
De eerste probe zat *in* `Queue::read()`: een timeout van een seconde met een
`ESP_LOGE` erop. Gedrag verder identiek, dacht ik. Gevolg: niet meer te
reproduceren. Een meting midden in de code die je verdenkt, verschuift precies
het venster dat je wilt zien.

Wat wel werkte, was er van buitenaf naar kijken. Een losse taak op prioriteit 3
die niets aanraakt en zwijgt tot er iets niet klopt, met als kenmerk: het
event-bit van de commando-queue staat aan terwijl die queue leeg is. En
daarnaast een schakelaar die het vermoede venster expres wijd openzet - een
`taskYIELD()` tussen `xQueueSend` en `setEventBits`
(`CRT_QUEUE_WIDEN_DOORBELL_RACE`, staat er nog in en is normaal uit).

Met die schakelaar aan liep het bij het tweede spel vast, en de dump was
eenduidig:

```
AsyncDisplay  blocked  bits=0x000003 (queues=0x000001 flags=0x000004 timers=0x000002)
queuediepte=0  queueBitGezet=1
buttonStart:  shown=1 enabled=1 pressed=0
```

Let op wat daar staat: zowel het queue-bit als het 50 ms-timerbit staat *aan*,
en toch blijft de taak blocked. Wie op een event group wacht wordt door zo'n bit
onmiddellijk vrijgegeven - dus die taak wacht helemaal niet op de event group,
maar een laag dieper, in `xQueueReceive`. En de knop was kerngezond: zichtbaar,
enabled, niet ingedrukt. Daarmee vielen alle andere verdenkingen (vastzittende
`bPressed`, vastzittende touchdetectie, blijven staan clip-rect) af.

### De oplossing
Het event-bit is een aanwijzing, nooit een belofte; de queue zelf is de enige
waarheid.

- `crt_Queue.h`: `tryRead()` erbij (leest zonder ooit te blokkeren) en een
  private `syncEventBit()` die het bit gelijktrekt met wat er werkelijk in de
  queue staat. Die kijkt na het wissen nog een keer, want tussen kijken en
  wissen kan een schrijver er alsnog iets in gezet hebben, en zijn belletje zou
  dan weggepoetst worden.
- `crt_AsyncDisplay.h`: de leeslus gebruikt `tryRead` en stopt als de queue toch
  leeg blijkt. `drainOneCommand()` idem.
- Bij `read()` staat nu met zoveel woorden dat hij niet na een `hasFired()` op
  diezelfde queue gebruikt mag worden.

Dezelfde combinatie - `waitAny` op een queue, dan `hasFired`, dan een
blokkerende `read` - stond ook in `crt__io_PetInTunnelDetector.h` en in de
voorbeelden TouchscreenKeyboard, AllWaitables en HasFired. Allemaal omgezet. De
plekken met `wait(queue)` gevolgd door `read()` (TestQueue, TestQueue2,
ServoSweeper) zijn met rust gelaten: die taken wachten op niets anders, dus een
belletje dat niets dekt laat ze hooguit op het volgende echte item wachten.

**Eerlijk over het bewijs:** natuurlijk reproduceren is met de meetopstelling
erin nooit gelukt - honderd versnelde spellen leverden niets op. Wat er ligt is:
het mechanisme bestaat aantoonbaar en geeft exact dit beeld, met de schakelaar
aan liep het binnen twee spellen vast, met de fix erin overleeft het diezelfde
schakelaar, en een lange normale sessie is schoon.

### Bijvangst: het donkere blokje over de startknop
Bij het opstarten viel er een klein donker vlak over de startknop. `Panel::hide()`
en `Text::hide()` tekenden hun eigen vlak leeg *ook als het widget nooit getoond
was*. Alle drie de pagina's verbergen zichzelf meteen na het opbouwen, dus alle
drie wisten daarbij vlakken waar nog nooit iets van hen had gestaan. Het
wisvlak van `paddlePanel` is (135,167)-(185,179) en `buttonStart` beslaat
(32,134)-(288,188): dat past er precies in. Omdat de commando's van de drie
taken in dezelfde queue door elkaar lopen, kon de grote achtergrondvulling van
het speelveld nog voor de knop landen en het kleine peddelvlakje er net na.

`hide()` wist nu alleen nog wat er ook echt getekend is (`isShown()`). Dat is
sowieso wat de kop van die functies belooft: "hide immediately undraws it".

### Wat er is blijven liggen
Drie taken tekenen bij elke overgang tegelijk in dezelfde commando-queue, zonder
enige ordening; de `vTaskDelay(200)` in `STATE_GAME_OVER` is een gok en geen
afspraak. `AsyncDisplay::waitUntilDrawn()` bestaat daarvoor. Het donkere blokje
was daar het zichtbare gevolg van; de tekenvolgorde blijft in principe
onbepaald.

Verder twee kleinigheden in `crt_PaddleControl.h`: `hasFired(flagLeftReleased)
|| hasFired(flagRightReleased)` kortsluit, zodat de tweede vlag nooit
geconsumeerd wordt, en een peddel die middenin een beweging wordt uitgezet laat
het eventbit van `repeatTimer` aanstaan - de eerste stap van het volgende spel
komt daardoor meteen.

## 2026-08-01 — De ingebouwde fouten uit pong_cleangui

`apps/pong_cleangui/README.txt`: *"Er zijn alleen een paar bugs gemaakt. Kun jij
ontdekken hoe het zou moeten werken, en de bugs oplossen?"* Dat is de oefening.
De console-variant `apps/pong/pong` heeft dezelfde README en blijft ongemoeid.

### Eerst: hij compileerde niet meer
De CleanGUI-API is doorgelopen sinds deze app is geschreven.
`TFT_eSPI_DisplayAdapter<N>` heet nu `TFT_eSPI_DisplayAdapter_RGB565<N>`, en de
`TouchscreenButton`-constructor heeft er drie parameters bij gekregen
(`bInvertedArea` en de twee case-offsets). Drie knoppen aanvullen, een naam
veranderen.

Lastiger was het includepad. De twee pong-apps dragen *dezelfde* headernamen —
`crt_BallControl.h`, `crt_GameControl.h`, `crt_PaddleControl.h`,
`crt_SceneDisplayControl.h`, `crt_CharMatrix.h`. Alleen `apps/pong/pong` stond in
`COMPONENT_ADD_INCLUDEDIRS`; `apps/pong_cleangui` erbij zetten zou betekenen dat
de volgorde in het CMake-bestand stilzwijgend bepaalt welke versie er gecompileerd
wordt. Er wordt toch maar een app tegelijk gebouwd, dus staat er nu een regel aan
en een uit, met een opmerking waarom. De `.ino` heet daarnaast `Pong_CleanGUI.ino`
(en de header `Pong_CleanGUI_ino.h`), zodat de twee entry points niet meer botsen.

### De kern: een raster dat pixels was geworden
`SceneDisplayControl` overschreef in zijn constructor `width`/`height` met
`display.getScreenSize()` — 320x240 pixels — terwijl de klasse
`SceneDisplayControl<20,10>` heet en `WIDTH`/`HEIGHT` verder nergens meer
gebruikt werden. `BallControl` en `PaddleControl` vragen die maat op via
`getWidth()/getHeight()` en rekenen er hun rasterlogica mee. Gevolg: de bal liep
een pixel per 500 ms — van het midden naar de rand ruim een minuut — en de
trefzone van de peddel was 3 pixels breed tegen een peddel van 100.

Dode template-parameters naast een waarde die duidelijk van hen was: dat is de
plek waar de fout is ingezet. `width`/`height` staan weer op `WIDTH`/`HEIGHT`, en
`getScreenSize()` wordt nog maar voor een ding gebruikt: de celgrootte
(320/20 = 16 bij 240/10 = 24 pixels).

Daar hoorde een tweede helft bij. De y-as was nooit omgeklapt. In de
console-versie deed `CharMatrix::setChar()` dat — `matrix[HEIGHT-1-y][x]`,
"oorsprong is linksonder" — en CleanGUI telt van linksboven. De spellogica ging
nog steeds uit van linksonder (peddel op y=0, bal gemist bij `posY<=0`, plafond
bij `posY==height-2`), dus belandde de peddel *bovenaan* en viel de bal naar boven
het veld uit. Er is nu een `cellCentreToPix(x,y)` die beide dingen tegelijk doet:
cel naar pixel, en y omklappen. Die ene functie is de enige plek die weet dat het
scherm van boven telt.

### De kleinere
- `BallControl::poolPaddlePosition` werd nooit geinitialiseerd, terwijl
  `SceneDisplayControl` dat voor zijn eigen pools wel deed. BallControl dacht dus
  dat de peddel op 0 stond tot de speler voor het eerst links of rechts aanraakte.
  In de console-versie viel dat niet op omdat start- en peddelknop daar dezelfde
  knop zijn; hier is de startknop apart, dus was de eerste bal gegarandeerd mis.
- `GameControl` riep bij de start `paddleControl.enable()` aan maar bij game over
  nooit `disable()` — de peddelknoppen bleven over het startscherm heen staan.
- `flagStartPressed` werd voor het wachten gewist, `flagBallMissedByPaddle` niet.
- `initialiseRandomStartPos()` meldde de nieuwe positie niet aan het scherm, dus
  begon elk spel met de bal nog even op de plek waar de vorige verloren ging. En
  `rand()` werd nooit geseed: elke boot dezelfde "willekeurige" richting.
- De initialisatielijst van `ballPanel`/`paddlePanel` las `width`/`height` terwijl
  die leden er nog niet waren. De panels krijgen hun maat nu in `main()`, zodra de
  celgrootte bekend is.
- Twee debug-logregels eruit: `updatePaddlePosition()` logde elke aanroep en
  `STATE_ENABLED` elke lus-iteratie. De logger is 100 regels diep; dat duwde eruit
  wat je met de dump-knop wilde zien.

### Twee dingen die pas op het bord bleken
Na de eerste flash: de startknop stond met zijn midden tegen de rechterrand, er
was wel een linker- en een bovenwand maar geen rechter, en de bal bewoog niet
terwijl het spel na een minuut wel netjes game over meldde. Allebei terug te
voeren op een verkeerde aanname over CleanGUI.

**`locPos` is de linkerbovenhoek. `Alignment` doet daar niet aan mee.**
`getGlobPosPix()` is domweg `parentPos + locPosPix`; de alignment wordt alleen
geraadpleegd bij het tekenen van een `Image` binnen een `Panel` en bij het
plaatsen van een `Text`. Een knop van 800 promille breed op x=500 promille begint
dus in het midden en loopt rechts van het scherm af. En `rightBorderPanel` met
locPos (0,0) en `Alignment::TopRight` komt gewoon op (0,0) terecht, boven op de
linkerwand — vandaar de verdwenen rechterwand. Alle posities worden nu expliciet
als linkerbovenhoek uitgerekend.

**`setLocPos()` verzet, `moveTo()` verzet en tekent.** Dat staat er met zoveel
woorden bij: "Deliberately NOT hide() + setLocPos() + show()". De bal en de peddel
gebruikten `setLocPos()`, dus stond de spellogica er goed bij terwijl er op het
scherm niets bewoog. Dat verklaart ook waarom uitgerekend de bal bleef staan toen
de rest verdween: bij `hide()` wist een paneel zichzelf op zijn *opgeslagen*
positie, en die was inmiddels weggelopen van de plek waar hij getekend was.

Daar hangt nog iets aan vast. `moveTo()` laat de vrijgekomen strook door de boom
opnieuw tekenen, en een `PageRoot` tekent zelf niets. Zonder achtergrondpaneel
blijft de oude bal dus staan — dezelfde vondst als bij de vier voorbeelden van
28 juli. Elke pagina heeft er nu een.

Twee dingen die daarbij opvielen en ook fout waren: `pageStart` werd een keer voor
de lus getoond en bij de start verborgen én uitgeschakeld, maar in
`STATE_GAME_OVER` nooit teruggezet — na een spel was er geen weg meer terug. En
`addChildWidget()` eist dat de ouder al een display heeft: eerst het paneel aan de
pagina hangen, daarna pas de tekst aan het paneel. Andersom is het een assert en
een bootlus.

Het "Game Over" staat nu op het scherm (een paneel met een `Text`), niet meer als
ASCII-art in de seriele monitor. Het speelveld beslaat de bovenste driekwart
(cellen van 16 bij 18 pixels), de peddelknoppen de onderste kwart, zodat ze elkaar
niet meer overlappen.

### Wat er nog opvalt
De app vraagt rotatie 1, terwijl `/TouchCalData1` op de spiffs rotatie 3 bevatte
van een vorige app. Dat forceert bij het eerste opstarten een hernieuwde
touchkalibratie. Allebei zijn landschap, dus `getScreenSize()` geeft in beide
gevallen (320,240) — het is puur de gedeelde kalibratie die erover valt. Een keer
kalibreren en het is over, of rotatie 3 aanhouden zoals MultiPanel doet.

## 2026-07-28 — Alle voorbeelden in PAL8, en een palet dat een mens kan lezen

Alle acht voorbeelden zijn op de PAL8-adapter gezet en een voor een op de CYD
gedraaid. Dat leverde vooral iets anders op dan verwacht: niet fouten in de
adapter, maar fouten in de *voorbeelden* die de directe adapter al die tijd had
toegedekt.

### Waarom de magenta index zoveel vond
`begin()` maakt het scherm een keer zwart, en op zwart valt een stuk scherm dat
door niemand wordt getekend niet op. Een backbuffer die met de transparante index
begint wel: die is magenta. Drie keer dezelfde soort vondst op een dag —
MultiPanel had het vorige week, vier voorbeelden bleken helemaal geen
achtergrondpaneel te hebben (AsyncDisplay, TouchscreenButton,
TouchscreenButtonGroup, TouchscreenKeyboard), en TouchscreenKeyboard riep
bovendien `touchscreenKeyboard.show(true)` in plaats van `page1.show(true)`,
zodat het paneel dat er inmiddels wel was nooit aan de beurt kwam.

Dat laatste was fraai zichtbaar: bij enter *werd* het gebied wel zwart, want de
`hide()` van het toetsenbord liet de ouder herstellen. Het paneel bestond dus,
het was alleen nooit getoond.

### present() stond in maar twee van de acht
Panel en MultiPanel riepen `present()` aan; de andere zes zijn geschreven voor de
directe adapter, waar die leeg is. Ze tekenden keurig in de buffer en stuurden
hem nooit uit - zwart scherm. Per voorbeeld een regel toevoegen zou zes keer
hetzelfde zijn en de zevende keer vergeten worden, dus het zit nu in
`AsyncDisplay::main()`: is de commandoqueue leeg, dan is het beeld af.

```cpp
if (bBufferDirty && (queueCommands.getNofMessagesWaiting() == 0) && display.isBuffered())
```

Drie voorwaarden, elk met een reden. `bBufferDirty` staat uit zodra het laatste
uitgevoerde commando zelf een `present` was, anders stuurt MultiPanel het scherm
twee keer per beeld uit en dat is 31 ms per keer. De queue-check voorkomt dat een
half beeld eruit gaat als de batchlimiet van tien commando's wordt geraakt. En
`isBuffered()` is onwaar bij RGB565, dus daar verandert er niets.

### Panel had te weinig stack
Boot loop met een `LoadProhibited` in een malloc diep in `TFT_eSPI::init()`.
Panel is het enige voorbeeld dat zijn adapter als *lokale* variabele in `main()`
maakt, dus die staat op de taakstack. Met de compiler nagemeten:
`sizeof` RGB565-adapter 984 bytes, PAL8 **2744** - de drie paletkopieen erin zijn
samen al 1280. Op 4000 bytes stack past dat niet meer; staat nu op 10000.

### Het palet: transparant naar 255, en een handgeschreven _userPalette.h
Aanleiding was dat er over het palet niets viel *af te spreken*: median-cut over
de plaatjes bepaalde alles, dus `paletteIndex(N)` was onbruikbaar zodra er een
plaatje bijkwam. De indeling is nu:

```
0 .. N-1       letterlijk uit _pal8/_userPalette.h   (ligt vast)
N .. N+M-1     median-cut over de plaatjes
N+M .. 254     opvulling: grootste kubus die past, daarna grijzen
255            transparant (magenta)
```

Transparant is naar het *eind* verhuisd zodat `arUserPalette[i]` samenvalt met
index `i`. Dat is de hele ergonomische winst; met transparant op 0 schoof alles
een op.

`arUserPalette` staat in ARGB (`0x00RRGGBB`), niet in RGB565 - het bestand is er
juist om met de hand geschreven te worden, en zo kun je een kleur letterlijk uit
een widget overnemen. De converter rekent om.

De opvulling is bewust geen zwart. `indexForColour()` zoekt over alle 255
ingangen, dus een staart vol zwart trekt elke donkere kleur naar puur zwart, ook
als de gebruiker bewust geen zwart koos. Een kubus dekt de ruimte gelijkmatig.
Bij MultiPanel gebeurde dat meteen: de quantizer gaf maar 156 kleuren terug op
255 gevraagd, dus 99 ingangen kregen kubus en grijzen.

Wat dit oplost: de widgets leiden **geen** kleuren af tijdens het draaien - `grep`
op `darken|lighten|blend` over `libs/CleanGUI/src` geeft niets, de bevelkleuren
zijn letterlijke waarden. Staan die exact in het palet, dan is de nearest-match
voor de hele GUI exact in plaats van benaderd. Precies wat Inputfield nodig had,
want dat voorbeeld leent zijn plaatjes van Panel en kreeg daarvoor een palet dat
uitsluitend uit een gradient en een ring was gekwantiseerd.

Magenta (`0x00FF00FF`) staat er met opzet *niet* in: het is nergens een
tekenkleur, alleen een transparantiemarkering, en in het palet zou het de
"magenta betekent niet-getekend"-diagnose ondermijnen.

Bijbehorend: `allocateBackBufferIfNeeded()` vult de buffer nu met index 255 in
plaats van 0, anders was die diagnose weg zodra index 0 een gewone
gebruikerskleur werd.

### De bevelkolom van het invoerveld
Klein artefact in Inputfield: zodra je typte werd de linker bevelkolom van het
invoerveld vanaf een bepaalde hoogte weggeveegd. Twee metingen brachten het
thuis - eerst de geometrie van `drawStringOpaque`, daarna de pixels zelf:

```
y=24: ... 3   3   3 ...     bovenrand van het veld (index 3 = 0x303030)
y=25: ...(x=32) 3  10 ...   alleen x=32 nog donker
y=32: ...(x=32)10  10 ...   hier is de bevelkolom weg
```

`Text::calcPaddingSizePix()` kortte de *hoogte* aan twee kanten in (`2*m`) maar de
*breedte* maar aan een kant (`m`), en die ene ging er rechts af. Links bleef er
niets over, dus de gereserveerde box begon precies op de bevelkolom. Verticaal
ging het daardoor al goed en horizontaal niet - vandaar dat alleen de linkerrand
sneuvelde.

Opgelost door het tekstdatum bij opake tekst een marge naar binnen te schuiven
(`calcOpaqueOffsetXPix()`); de adapter leidt de box uit dat datum af, dus die
schuift mee. De breedte gaat nu met `2*m` omlaag, want anders loopt de box aan de
overkant weer over de bevel. Gecentreerde tekst blijft ongemoeid: die ligt al
symmetrisch om zijn datum.

Waarom pas een lege regel het herstelde: de rechterstrook begint op
`textTopLeft.x + textWidthPix` en raakt kolom 32 nooit - behalve bij lege tekst,
want dan begint hij op 32 zelf.

### Schuifanimatie met een deadline
Het toetsenbord van Inputfield schoof zo snel binnen dat er niets van te zien
was. Geen vaste pauze per stap, maar een deadline per stap met `waitUntilDrawn()`
ervoor: een `present()` kost hier al ~31 ms per stap en bijna niets bij
rechtstreeks tekenen, dus een vaste pauze zou de duur adapterafhankelijk maken.
Nu op 400 ms - en dat is ongeveer de vloer, want ~13 stappen x 31 ms is al 0,4 s.
Lager zetten verandert niets meer; daarvoor moet `stepPix` omhoog.

### Gemeten en afgevinkt
`getScreenSize()` geeft inmiddels gewoon 320x240 - het openstaande punt uit de
entry van 2026-07-26 is in de adapter rechtgezet.

### Twee bestaande fouten in ImageViewer, allebei door meten gevonden
Geen van beide had met het palet te maken; ze zaten er al.

**Een knop bleef na indrukken voorgoed verzonken.** De log gaf het meteen weg:
alleen `pressed=1`, nooit een `pressed=0`. Het loslaten kwam dus niet aan.

`TouchscreenButton::enable()` wiste `bPressed`, en `Widget::show()` roept
`enable(false)` aan. Wie de knop ingedrukt tekende, wiste daarmee tegelijk het
feit dat hij was ingedrukt, waarna `touchReleased()` niets meer stuurde -
`if (bPressed)` was al onwaar. Wissen gebeurt nu alleen nog bij een echte
overgang van uit naar aan, wat de oorspronkelijke bedoeling van die regel was.

**De labels van `r-` en `c-` ontbraken tot je ze indrukte**, die van `r+` en `c+`
niet. Die asymmetrie was de aanwijzing. Gemeten in `redrawRegion()`:

```
regio  pos=( 89,157) size=(141, 77)
r-     pos=(136,157) size=(  0,  0)
c-     pos=( 89,185) size=(  0,  0)
```

*Alle vier* de labels hebben maat nul, maar de cull-test
`rectsIntersect(getGlobPosPix(), _sizePix, ...)` wordt daarmee
`regioTop < labelY` en `regioLinks < labelX`. `r-` ligt op de bovenrand van de
hertekende regio en `c-` op de linkerrand, dus daar staat `157 < 157` en
`89 < 89`. De plus-knoppen liggen verderop en overleefden het toevallig. Rakende
randen tellen met opzet niet mee (zie `crt_typesAndConstants.h`), en dat is
precies wat een rect van niets fataal maakt.

Waarom de maat nul was: `Text::updateSize()` draait alleen vanuit `setFont`,
`setFontScale` en `setText`, en die laatste alleen als de string *verandert*. Een
label dat zijn tekst bij constructie krijgt en nooit wijzigt, komt langs geen van
de drie - en bij constructie is er ook nog geen display, waar `updateSize()`
sowieso op afbreekt. `Text` haakt nu in op `setDisplay()`: het eerste moment
waarop de tekst te meten valt.

### Werkwijze die zich terugbetaalde
Vier artefacten, vier keer eerst meten. Elke keer wees de redenering vooraf de
verkeerde kant op:

- Blauwe randjes bij groene knoppen: er bleek helemaal niets te blenden - de
  adapter kreeg maar drie kleuren binnen en de buffer had harde overgangen. Het
  was de magenta achtergrond ernaast, die tegen groen wel opvalt en tegen rood
  niet.
- Het kolommetje in het invoerveld: niet de blanking (die tekende daar nul
  pixels) maar de gereserveerde box, die op de bevel begon.
- Zwart scherm bij TouchscreenButtonGroup: een los stekkertje.
- De labels: niet de regio - die klopte precies - maar een rect van niets.

Instrumentatie die daarbij hielp en er weer uit is: kleur-naar-index in
`indexForColour()`, een pixeldump uit `present()`, de geometrie van
`drawStringOpaque()`, en de cull in `redrawRegion()`.

### 80 MHz geprobeerd, en afgesloten
Het laatste openstaande punt uit de speed-research. `SPI_FREQUENCY` op 80 MHz
(80/1) gezet en MultiPanel gedraaid:

| sprites | 40 MHz | 80 MHz |
|---|---|---|
| 0 | 29 | **51** |

Doorvoer is dus geen probleem - het bord komt gewoon op en meldt vrolijk 51 fps.
**Maar het beeld is stuk.** Dat is precies het falen dat we verwachtten: niet een
crash, maar ruis en verkeerde kleuren, terwijl de seriële poort blijft doen alsof
er niets aan de hand is. Zonder ernaar te kijken had dit als winst geteld.

Iets ertussenin bestaat niet. De ESP32 deelt de 80 MHz APB-klok door een geheel
getal, dus 80/1, 80/2 (=40), 80/3 (=26,67) en 80/4 (=20) zijn de enige echte
waarden; voor iets tussen 40 en 80 zou de deler tussen 1 en 2 moeten liggen. Een
`60000000` in User_Setup.h levert stilzwijgend 40 MHz op. Terug op 40 dus, met de
reden erbij in het bestand zelf.

Wel iets geleerd over waar de tijd nu zit. Bij 80 MHz halveert de busvloer van
31,6 naar 15,8 ms, wat zuiver busgelimiteerd ~63 fps zou geven. Gemeten werd 51
fps = 19,6 ms. Dat verschil is het uitpakken van het palet, dat bij 40 MHz
volledig onder de transfer wegviel en er nu bovenuit steekt. **De bus is niet
langer het enige plafond** - een aardige correctie op de vuistregel in CLAUDE.md
dat vrijwel al het tekenwerk busgelimiteerd is. Die geldt bij 40 MHz; bij 80 niet
meer.

### Nog open
Niets meer uit de oude lijst.

## 2026-07-28 — DMA: uitpakken en uitzenden tegelijk

`present()` was de rem: 38,4 ms, waarvan 31,6 ms busvloer en ~6,8 ms CPU (het
palet toepassen, en de FIFO voeren). Met DMA hoeven die niet meer op elkaar te
wachten - terwijl strook N over de bus gaat, pakt de CPU strook N+1 uit. Het
logboek zei het al: *"DMA loont pas als er ook CPU-werk per frame is om mee te
overlappen."* Dat was er nu eindelijk.

### Wat het werd
- `initDMA()` zodra het paneel draait, en `startWrite()` erachteraan om CS laag te
  houden. Dat laatste is nodig omdat `setAddrWindow` hem anders tussen de stroken
  door loslaat, en omdat `endWrite()` op de laatste transfer zou wachten - precies
  de wachttijd die we willen vermijden. Buiten `present()` raakt niets anders het
  paneel: al het tekenwerk gaat naar de buffer.
- **Twee** strookbuffers uit `MALLOC_CAP_DMA` (20 KB), waar `present()` tussen
  pingpongt. De `dmaWait()` zit binnenin `pushImageDMA` en wacht op de *vorige*
  transfer, niet op de zijne; dat is precies de pijplijn.
- De const-variant `pushImageDMA(x,y,w,h, const uint16_t*)` met opzet: die
  kopieert niet en wisselt geen bytes om, en dat laatste hoeft niet want het palet
  staat al omgedraaid klaar.
- Mislukt `initDMA()`, dan pusht `present()` gewoon blokkerend verder.

### Het risico dat niet uitkwam
`initDMA()` roept `spi_bus_initialize(HSPI_HOST, ...)` aan met `ESP_ERROR_CHECK`
eromheen: was die bus al door de Arduino-laag geclaimd via dezelfde IDF-driver,
dan was dat geen foutcode maar een abort. Daarom eerst alleen dát aangezet en
geflasht. Hij geeft `true` en het bord komt gewoon op - de Arduino-laag praat via
`esp32-hal-spi` met de registers, niet via `spi_master`.

De touch kan er ook niet mee in de knoop raken: met `TOUCH_SCLK` gedefinieerd
gebruikt `Touch.cpp` zijn eigen `touch_spi_transfer()` met `digitalWrite`, op
eigen pinnen. De `spi.transfer`-tak eronder is voor borden die de displaybus
delen; die van ons niet.

### Gemeten

| sprites | zonder DMA | met DMA |
|---|---|---|
| 0 | 24 | **29** |
| 5 | 21 | 27 |
| 20 dekkend | 20 | 26 |
| 20 transparant | 18 | 22 |

Zo'n 25% erbij over de hele linie, en de curve blijft vlak: 29 -> 22 over nul tot
twintig sprites. Ter herinnering waar dit begon: dezelfde twintig transparante
sprites deden 3 fps op de directe adapter met `moveTo`.

`present()` zit nu op ~33 ms tegen een busvloer van 31,6. Daarmee is dit het
eindpunt van deze weg: zonder een tweede SPI-bus of een hogere klok valt er niets
meer te halen.

## 2026-07-27 — De PAL8-backbuffer werkt

`TFT_eSPI_DisplayAdapter_PAL8`: een schermvullende 8bpp-sprite waarvan wij de
bytes als palet-indices lezen. Tekenen gaat via TFT_eSPI zelf - inclusief tekst
met free fonts, wat de reden was om op een `TFT_eSprite` te blijven in plaats van
een eigen `uint8_t`-buffer.

### De dragerkleur
Elke 8bpp-schrijfactie in TFT_eSPI reduceert de kleur met
`(c & 0xE000)>>8 | (c & 0x0700)>>6 | (c & 0x0018)>>3`. Die is exact omkeerbaar,
dus voor elke index bestaat er een kleur die naar precies die index reduceert.
`colTo16bit()` - nu virtueel - geeft die dragerkleur terug in plaats van een echte
kleur, en TFT_eSPI schrijft daarmee onze index.

Bij het opstarten worden alle 256 indices erin geschreven en met
`readPixelValue()` teruggelezen. Lukt dat niet, dan valt de adapter terug op het
paneel en zegt waarom. De aanname over TFT_eSPI's binnenwerk is daarmee een
gecontroleerd feit; zonder die controle zou een toekomstige versie van de
bibliotheek dit stilzwijgend kunnen breken.

Vijf primitieven (`fillScreen`, `drawRect` en de drie rounded rects) heeft een
sprite niet zelf en zijn niet virtueel, dus die zijn hier opgebouwd uit
`fillRect`. Afgeronde hoeken zijn voorlopig vierkant.

### Gemeten, met de auto-spawn van eerder

| sprites | RGB565 | PAL8-backbuffer |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 10 dekkend | 39 | 20 |
| 20 dekkend | 16 | 14 |
| 20 transparant | 3 | 4 |

Wat hier staat is precies wat een backbuffer doet: **de beeldsnelheid wordt
vlak.** Van 0 naar 20 sprites zakt hij van 24 naar 14 fps, waar de directe
adapter van 100 naar 16 duikelt. Bij weinig op het scherm is de backbuffer dus
duurder en bij veel goedkoper - het omslagpunt ligt rond de 20 dekkende sprites.
En het flikkeren en de wisselende volgorde zijn weg, want er komt pas iets op het
scherm als het beeld af is.

### Index 0 is magenta, en dat is met opzet
In het palet dat de converter schrijft is index 0 gereserveerd voor transparant.
Die index wordt bij het blitten overgeslagen, dus je hoort hem nooit op het scherm
te zien - en juist daarom staat er magenta (0xF81F) in plaats van zwart.

Dat betaalde zich meteen terug. Bij de eerste PAL8-run was de strook rond de
knoppen in MultiPanel paars. Geen fout in de adapter: die pixels werden door geen
enkele widget getekend. De map loopt tot y=192, de knoppen beginnen op 196, en
daartussen was het scherm van niemand. Op het paneel viel dat nooit op, omdat
`begin()` het scherm een keer zwart maakt en dat zwart daar toevallig goed
uitzag. Een backbuffer die met index 0 begint, maakt zo'n gat zichtbaar.

Was de buffer met "zwart" geinitialiseerd, dan had het gat er nog steeds gezeten
en had niemand het gezien - tot het moment dat de achtergrondkleur verandert.
MultiPanel heeft nu een schermvullend `backgroundPanel` onderop, zoals ImageViewer
al had.

Dezelfde regel geldt breder: **magenta in beeld betekent dat er iets niet is
getekend.** Niet dat de kleur mis is.

### Waar de rest van de tijd zit
Van de ~71 ms bij 20 dekkende sprites is ~40 ms de uitzending en ~31 ms het
tekenen in de buffer.

De transparante blit is apart gemeten, in de adapter zelf:

```
transp 64x64: 599 us per blit, 173 stukken, 3463 ns per stuk
```

Van die 599 us gaat vrijwel alles op aan de 173 aanroepen van
`TFT_eSprite::pushImage` (3,5 us elk). Het verplaatsen van de pixels zelf stelt
niets voor: 1.945 zichtbare bytes is bij DRAM-tempo een paar microseconden.
Anders gezegd: **99% van een transparante blit is opzet, 1% is werk.**

Die 3,5 us zit in de aanroep zelf, de `PI_CLIP`-macro die per aanroep opnieuw
tegen de viewport klipt, de keten van `_bpp`/`sbpp`-tests, en een `memcpy` van
gemiddeld elf bytes - die op zijn beurt ook aanroepkosten heeft.

Eerder stond hier 45 us per stuk. Dat was afgeleid uit de beeldsnelheid en dus
uit alles wat er verder in een beeld gebeurt; de directe meting zegt 3,5.

### En toen bleek het ergens anders te zitten
De rekensom klopte nog steeds niet: 40 ms uitzending plus ~15 ms blits is 55, en
er ging 105 ms in een beeld. Dus per commandotype opgeteld aan de tekenkant, met
een samenvatting per seconde in plaats van een logregel per commando:

```
per seconde: tekenen 923 ms, wachten 29 ms, overig 51 ms
   drawImage    n= 797    509 ms  (638 us elk)
   present      n=  10    384 ms  (38417 us elk)
   fillRect     n= 560     17 ms  (31 us elk)
```

**797 blits per seconde bij tien sprites en tien beelden per seconde: 75 blits
per beeld.** Niet tien. Elke transparante sprite laat via `repaintRegion` zijn
hele 64x64 opnieuw opbouwen, inclusief elke buur die daar overheen valt, plus de
vrijgekomen randstroken. Dat is de O(N^2) uit het z-verhaal van eerder, nu met
een getal erbij.

### De les: met een backbuffer is slim hertekenen dom
De hele `moveTo`-machinerie bestaat om buspixels te sparen. In een buffer is er
geen bus, en betaal je alleen de nadelen: buren die onnodig meegetekend worden.
Gewoon per beeld de hele tree opnieuw opbouwen kost tien blits in plaats van
vijfenzeventig.

`IDisplay::isBuffered()` erbij, zodat een voorbeeld eenmalig kan vragen wat voor
display het heeft en zijn aanpak daarop kiest. MultiPanel doet nu:

```cpp
if (_bDisplayIsBuffered) { setLocPos(...); }  // en daarna page1.show(true)
else                     { moveTo(...); }     // alleen de vrijgekomen strook
```

De adapter moest daarvoor het aanmaken van de buffer scheiden van het omleiden
van het tekenwerk: het antwoord op `isBuffered()` mag niet afhangen van wie er
toevallig het eerst wakker is, maar de kalibratie moet wel op het paneel blijven
tekenen omdat ze om aanrakingen vraagt.

### Eindresultaat

| sprites | RGB565, moveTo | PAL8-backbuffer, alles per beeld |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 5 | - | 21 |
| 10 dekkend | 39 | 21 |
| 20 dekkend | 16 | 20 |
| 20 transparant | 3 | **16** |

Van 3 naar 16 beelden per seconde, en de curve is bijna vlak: 24 -> 16 over nul
tot twintig sprites, waar de directe adapter van 100 naar 3 duikelt. De
commandowachtrij zakte van ~500 naar ~31, want de tekentaak loopt niet meer
achter de feiten aan.

Wat nu de rem is, is `present()` met 38,4 ms - het plafond ligt dus op 26 fps.

### De eigen blit, alsnog
Daarna toch gebouwd: klippen een keer per plaatje, dan `memcpy` per aaneengesloten
stuk rechtstreeks in de buffer, in plaats van `TFT_eSprite::pushImage` per stuk.

| 20 transparante sprites | fps |
|---|---|
| via `pushImage` per stuk | 16 |
| eigen klipping + `memcpy` | **18** |

Van de 62,5 ms per beeld ging ~24 ms naar tekenen, nu ~17 ms. Geen factor zes
zoals ik voorrekende: de resterende tijd zit in het aflopen van de rij op zoek
naar stukken (4.096 vergelijkingen per sprite) en in 173 kleine `memcpy`s. De
opzet per aanroep is weg, het scannen niet.

Drie begrenzingen gelden tegelijk en worden nu zelf uitgerekend: de rechthoek van
het panel, de clip-rect van `repaintRegion`, en de buffer. Dat is precies het soort
code waar een fout niet lelijk oogt maar buiten de buffer schrijft.

En dat pad werd door het voorbeeld nooit geraakt: de sprites ketsten netjes binnen
de map, en bij een volledige hertekening staat er geen clip-rect. Een eerste test
met sprites die 60 pixels over alle randen heen liepen bewees alleen dat het niet
crasht - visueel klopte hij niet eens, want CleanGUI klipt een kind niet tot zijn
ouder en die sprites tekenden dus over de knoppen heen.

De oplossing die het wel doet: **de sprites horizontaal met hun centrum op de rand
laten ketsen.** Daar valt de rand van de map samen met de rand van het scherm, dus
dan is het echte klipping, in normaal gebruik, en zichtbaar te controleren. Dat
staat nu zo in het voorbeeld - niet als test maar als gedrag, want ongebruikte
klipping is precies het soort code dat stilletjes verkeerd staat.

Verticaal blijven ze wel binnen de map: erboven ligt de fps-regel en eronder de
knoppen. Daarmee is `xFrom`/`xTo` in gebruik en gecontroleerd, en `yFrom`/`yTo`
niet. Zelfde code, andere as, dus het risico is klein - maar getest is het niet.

### Stand van zaken

| sprites | RGB565, moveTo | PAL8-backbuffer |
|---|---|---|
| 0 | 100 (de klokcap) | 24 |
| 10 transparant | - | 20 |
| 20 dekkend | 16 | 20 |
| 20 transparant | 3 | **18** |

Het plafond is nu `present()` met 38,4 ms, ofwel 26 fps. Wie verder wil: DMA laat
de expansie van de volgende strook overlappen met het uitzenden van de vorige, en
brengt dat richting de busvloer van 31,6 ms.

## 2026-07-27 — PAL8: gedeeld palet, indices in flash

Eerste twee stappen van de paletted weg. De keuze voor een palet in plaats van
RGB332 volgde uit de meting van hiervoor: de winst zat in de opzoektabel, en die
tabel heeft dezelfde vorm of je hem nu met het 3-3-2 raster vult of met
zelfgekozen kleuren. Een palet is dus gratis.

### Kleur of index, via één trechter
`paletteIndex(i)` in `crt_typesAndConstants.h` markeert de bovenste byte van een
ARGB-woord (die overal nul is) als "dit is een index". Daarmee betekent dezelfde
`fillRect()` zowel "maak dit lichtblauw" als "maak dit kleur 7", zonder de
interface te verdubbelen. Het landt allemaal in `colTo16bit()` van de adapter -
de enige plek waar een CleanGUI-kleur hardware-kleur wordt.

### De converter
`img2rgb565.py --pal8` leidt één palet af uit **alle** plaatjes van een
images-boom samen, en zet ze allemaal om naar indices daarin. Dat is met opzet
een operatie over de hele boom: alleen als plaatje en scherm hetzelfde palet
delen zijn de bytes van een plaatje al schermindices, en dat is wat blitten tot
kopieren terugbrengt. Index 0 is transparant, altijd - dat vervangt een
kleurvergelijking door een vergelijking met nul. Uitvoer in `images/_pal8/`,
naast de bestaande `_rgb565/`.

Voor de twee barbaren van MultiPanel: 156 kleuren uit 6.041 zichtbare pixels, en
8.192 bytes flash in plaats van 16.384.

### De les van vandaag: meet niet je eigen foutmelding
De eerste metingen zeiden dat een PAL8-plaatje 5,45 ms kostte tegen 1,88 ms voor
hetzelfde plaatje in RGB565. Bijna drie keer zo traag, en geen enkele verklaring
klopte: stroken pushen in plaats van regels hielp niet, het palet naar DRAM
verhuizen hielp niet, en de rekensom kwam op ~1,9 ms uit.

De oorzaak was dat er **helemaal niets werd getekend**. Bovenin `drawImage` stond:

```cpp
if (image.format != PixelFormat::RGB565)
{
    ESP_LOGI(..., "Unsupported PixelFormat: %d", ...);
    return;
}
```

PAL8 viel daar bij binnenkomst uit, en die 5,45 ms was de logregel zelf: op
115200 baud kost een regel van vijftig tekens ruim 4 ms. De meting was dus
volstrekt consistent (p90 lag 8 us van de mediaan) en volstrekt betekenisloos.

Die melding is nu eenmalig in plaats van per beeld, en de bewaking laat PAL8 door.
Wat de meting bruikbaar had gemaakt: de sprites waren onzichtbaar - dat was met
één blik op het scherm te zien geweest, en ik zat naar getallen te kijken.

### Wat het daarna wél doet
| | RGB565 | PAL8 op dezelfde adapter |
|---|---|---|
| 5 sprites | - | 40-60 fps |
| 20 dekkend | 16 fps | 8 fps |
| 20 transparant | 3 fps | 1 fps |

Ongeveer half zo snel, en dat hoort zo: het scherm is 16 bits, dus elk plaatje
moet alsnog per pixel door het palet, en transparant blitten kost een push per
aaneengesloten stuk. **PAL8 op een 16-bits doel is een compatibiliteitspad, geen
snelheidspad.** De winst komt pas als het doel zelf paletted is - dan is blitten
een `memcpy` per beeldrij. Dat is de volgende stap.

## 2026-07-27 — Aanzet tot een 8bpp backbuffer, en waarom hij (nog) niet werkt

### Wat er staat en werkt
- `IDisplayDraw::present()` door de hele keten: `AsyncDisplay` zet hem in de
  wachtrij, `DisplayCommand` voert hem uit, de directe adapter doet niets.
- De adapter is gesplitst in **het paneel** (`_tft`: begin, rotatie, touch,
  kalibratie) en **het tekendoel** (`_pTarget`). Hij heet nu
  `TFT_eSPI_DisplayAdapter_RGB565`, met een alias-template op de oude naam zodat
  `apps/pong_cleangui` en `CleanCore/examples/StdQueue` niets merken.
- Elk voorbeeld kiest met één regel:
  `using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_RGB565<...>;`
- `AsyncDisplay::waitUntilDrawn()`: wacht tot alles wat is ingestuurd ook is
  uitgevoerd. Nodig zodra present() duur wordt - zonder rem groeide de
  commandobuffer van 110 naar 843 in tien seconden (100 beelden per seconde
  insturen, 21 kunnen tonen) en sloeg de task watchdog aan. De **queue-diepte
  deugt niet als maat**: het commando dat op dat moment wordt uitgevoerd staat
  niet meer in de rij. Vandaar twee tellers.
- Alles geverifieerd: MultiPanel haalt met 20 dekkende sprites nog steeds 16-17
  fps, dus deze omweg kost in RGB565 niets.

### Waarom de RGB332-variant niet werkt
Aanname was: een `TFT_eSPI*` die naar een `TFT_eSprite` wijst tekent in die
sprite, want `TFT_eSprite` erft van `TFT_eSPI`. **Fout.** Van alle tekenmethoden
in TFT_eSPI zijn er precies acht virtueel - `drawPixel`, `drawChar`, `readPixel`,
`setWindow`, `pushColor` en drie hulpjes - met erboven: *"These are virtual so the
TFT_eSprite class can override them"*. De rest wordt door de sprite *verborgen*,
niet overschreven, dus via een basisklasse-pointer draait de paneelversie.

Van de 26 methoden die de adapter gebruikt: 16 zijn puur toestand (goed), 3 heeft
de sprite zelf (`fillRect`, `pushImage`, `width`), 2 lopen via de virtuele
`drawChar` (goed), en **5 landen op het paneel in plaats van in de buffer**:
`fillScreen`, `drawRect`, `drawRoundRect`, `fillRoundRect`,
`fillSmoothRoundRect`. Plus: `drawString` vult bij free fonts de glyph-cel met
`fillRect` (TFT_eSPI.cpp:5039) - ook niet-virtueel.

Vervolg zou zijn: de adapter templaten op het doeltype in plaats van een pointer,
plus eigen implementaties van die vijf, opgebouwd uit `fillRect`/`drawFastHLine`/
`drawPixel` die de sprite wel heeft.

### En het plafond ligt lager dan gedacht: gemeten uitsplitsing
Een schermvullende 8bpp-sprite uitsturen kost **46 ms, niet 31**. Dat is 21 fps,
geen 32. Waar die 14,6 ms extra in gaan (elk 76.800 pixels, gemiddelde van 5):

| meting | tijd | wat het toevoegt |
|---|---|---|
| `fillScreen`, egaal, geen brondata | 31.602 us | de busvloer |
| `pushImage` 16bpp, stroken van 8 regels (30 aanroepen) | 34.957 us | +3,4 ms: brondata ophalen |
| `pushImage` 16bpp, per regel (240 aanroepen) | 39.754 us | +4,8 ms: 210 extra aanroepen, ~23 us elk |
| `pushSprite` 8bpp, egale buffer | 46.209 us | +6,5 ms: de per-pixel lus |
| `pushSprite` 8bpp, ruis | 47.960 us | +1,75 ms: het schuifwerk zelf |

**De omzetting is niet de boosdoener.** Het rekenwerk (3-3-2 uitpakken naar
5-6-5) is met 1,75 ms het goedkoopste onderdeel; TFT_eSPI slaat het bovendien
over zolang de kleur niet verandert. Het geld zit in de per-pixel lus die twee
losse bytes in een regelbuffer schrijft (6,5 ms = 85 ns/pixel, zo'n 20 klokken
voor laden, vergelijken en twee byte-stores - `_lastColor` is een lid, dus geen
register) en in het per beeldregel pushen (4,8 ms).

Wat daaraan te doen zou zijn: een tabel van 256 `uint16_t` in plaats van
schuiven en vergelijken (één load, één 16-bits store, geen sprong), en pushen per
strook van 16 regels in plaats van per regel. Dat is vervolgens gebouwd en
gemeten, op dezelfde buffer vol ruis:

| | tijd |
|---|---|
| `pushSprite` van TFT_eSPI | 48.005 us |
| eigen lus: opzoektabel, stroken van 16 regels | **37.977 us** |
| alleen de expansie, zonder de bus | 3.421 us |

Tien milliseconden sneller, ofwel **26 fps in plaats van 21**. De optelsom klopt:
37,98 = 34,56 (bus met stroken) + 3,42 (expansie). Van de winst komt ongeveer de
helft uit de vorm van de lus (8,25 -> 3,42 ms) en de helft uit stroken in plaats
van losse regels.

**En daarmee wordt een palet gratis.** De expansie is bij een tabel niet meer dan
`*pDst++ = arLut[*pSrc++]`, en die tabel kan net zo goed 256 zelfgekozen kleuren
bevatten als het 3-3-2 raster. Precies dezelfde kosten per pixel. De reden om
voor RGB332 te kiezen was dat TFT_eSPI het kant en klaar had - maar dat kant en
klare is nu juist wat vervangen wordt. Paletted 8bpp, het oorspronkelijke idee,
is dus zowel mooier als even snel.

De busvloer van 31,6 ms (32 fps) haal je alleen met DMA, die de expansie van de
volgende strook laat overlappen met het uitzenden van de vorige. Dat scheelt nog
~6 ms.

## 2026-07-27 — MultiPanel: wat een bewegende sprite echt kost

Nieuw voorbeeld `CleanGUI_for_TFT_eSPI/examples/MultiPanel`: barbaren lopen over
een groene map, twee knoppen spawnen er een (transparant of dekkend), een derde
ruimt op, en bovenin staat de fps. Tot 20 per soort, vooraf aangemaakt en met
`setDrawable(false)` uit beeld tot ze spawnen.

### Het gat in moveTo dat dit blootlegde
`moveTo()` herstelde de vrijgekomen stroken via de root, maar tekende zichzelf op
de nieuwe plek *zonder* de root: *"whatever this widget covers, it covers"*. Dat
klopt niet voor een transparant plaatje — meer dan de helft van deze barbaar is
doorzichtig — en ook niet voor een panel met afgeronde hoeken. Zonder ingreep
laat een bewegende sprite een spoor van zichzelf achter.

Daarom `Widget::coversOwnRect()`, met `false` als veilige default en een
`Panel`-override die kijkt naar afgeronde hoeken en naar wat `needsBackground()`
al wist. `moveTo` kiest daarop het snelle pad of een `repaintRegion` door de
root. `TouchscreenButton` is beveld en dekkend, dus het schuivende toetsenbord
van Inputfield houdt het snelle pad.

### Gemeten (100 fps is de klokcap van de demo, geen plafond van het bord)

| sprites | transparant | dekkend |
|---|---|---|
| 4-5 | 53 fps (4,7 ms/sprite) | 99 fps (nog op de cap) |
| 10 | 16 fps (6,3 ms/sprite) | 39 fps (2,6 ms/sprite) |
| 20 | 3 fps (16,6 ms/sprite) | 16 fps (3,1 ms/sprite) |

Bij lage dichtheid klopt de raming van ~4 ms voor een transparante sprite. Maar
**transparant schaalt niet lineair en dekkend wel** — en dat is de echte les van
dit voorbeeld.

De reden: een transparante sprite laat zijn eigen rechthoek door de **hele tree**
opnieuw opbouwen. In die 64x64 zit niet alleen de map maar ook elke andere sprite
die er toevallig overheen valt, en die betaalt dan opnieuw zijn eigen runs. Met
20 sprites van 64x64 op een map van 320x172 beslaat het spriteoppervlak anderhalf
maal de map, dus elke verplaatsing tekent er gemiddeld nog een paar mee: kosten
die groeien met de *dichtheid*, niet met het aantal. Een dekkende sprite tekent
alleen zichzelf en blijft daarom netjes lineair; alleen zijn vrijgekomen stroken
gaan door de root, en die zijn dun.

De commandobuffer van `AsyncDisplay` stond bij alles boven de vijf sprites vol
(300). Dat is geen fout maar tegendruk: de tekentaak is de rem, de bus is de
bodem. Een grotere buffer maakt dus niets sneller.

### Waar de winst zit als dit ooit moet
Niet in sneller tekenen maar in minder samenstellen: de sprite met zijn
achtergrond in een klein DRAM-buffertje samenvoegen en die in één dekkende push
versturen. Dan valt zowel het per-run-tarief als het hertekenen van de buren weg.
Dat is een nieuw mechanisme in CleanGUI en dus een aparte ronde.

### Onderweg
`Widget<0>` liet gcc struikelen over `_arChildren[i]` op een array van lengte
nul (`-Werror=array-bounds`), zodra een `Panel<0>` als los object werd gebruikt.
Elke lus over de kinderen heeft nu `(MaxNofChildren > 0) &&` erbij; dat is een
template-parameter, dus de conditie valt bij het compileren weg.

### De stack van de AsyncDisplay-taak: gemeten, niet geraden
Elk voorbeeld gaf die taak
`20000 + CommandBufferSize*sizeof(DisplayCommand) + MaxNofClientTasks*sizeof(PrintContext)`
= 39.440 bytes. Met `dumpStackHighWaterMarkIfIncreased()` gemeten:

| example | belasting | stack over | gebruikt |
|---|---|---|---|
| MultiPanel | 20 sprites, transparante blits, clip-rects | 36.952 | **2.488** |
| Inputfield | free fonts, `drawStringOpaque` per toets, schuivend toetsenbord | 36.944 | **2.496** |

Twee heel verschillende commando-mengsels, 8 bytes uit elkaar. Logisch: die taak
haalt één commando uit de queue en voert het uit — er is geen diepe keten. De
watermark loopt vanaf taakstart, dus kalibratie en het mounten van LittleFS
zitten erin. Inputfield kon zonder touch gemeten worden dankzij
`CRT_INPUTFIELD_AUTOSLIDE` en `CRT_INPUTFIELD_AUTOTYPE`.

Beide termen in die formule telden geheugen mee dat niet op de stack staat: de
commandobuffer is een FreeRTOS-queue op de heap (`xQueueCreate`, `crt_Queue.h:29`)
en de `PrintContext`s zijn leden van `AsyncDisplay` (`sizeof` = 628). Alle zeven
voorbeelden staan nu op een kale `8000` — ruim drie keer de gemeten piek — en de
buffer ging van 300 naar 1000 commando's.

Per saldo: queue 19.200 -> 64.000 bytes heap, stack 39.440 -> 8.000. Netto ruim
13 KB erbij, van de ~200 KB die vrij was. `apps/pong_cleangui` heeft de oude
formule nog; dat is een app en geen voorbeeld, dus die is met rust gelaten.

Wat een grotere buffer *niet* doet: de beeldsnelheid verhogen. Zolang er meer
commando's worden geproduceerd dan de bus afvoert, loopt ook een buffer van 1000
vol — het duurt alleen langer. Wat hij koopt is speling bij pieken (spawnen, een
volledige paginatekening).

## 2026-07-27 — ImageViewer hertekent alleen nog wat verandert

### Wat er mis mee was
Elke plaatjeswissel tekende het hele scherm opnieuw: **twee** volledige
vullingen van 76.800 pixels, ~31 ms per stuk. De eerste kwam van
`backgroundPanel`, de tweede van `imagePanel` — dat was óók schermvullend, en een
panel dat groter is dan zijn dekkende plaatje vult eerst zijn `colFg`. Die
tweede overschilderde de eerste dus in dezelfde kleur. Plus caption en zes
knoppen: ~75 ms voor een wissel die soms maar 32x32 pixels verandert.

### De drie ingrepen
1. **`imagePanel` is nu precies zo groot als het plaatje** en wordt via zijn
   positie gecentreerd, niet via `Alignment::MidMid` binnen een schermvullend
   panel. Daarmee is `needsBackground()` onwaar en verdwijnt de tweede vulling.
2. **Alleen de veranderde rechthoek hertekenen.** `Widget::repaintBehind` is
   publiek geworden als `repaintRegion(topLeftPix, sizePix)` — de machinerie was
   er al voor het wegschuivende toetsenbord. De viewer onthoudt waar het plaatje
   staat en hertekent bij een wissel de vereniging van oude en nieuwe rechthoek.
   Omdat alles gecentreerd staat, is die vereniging simpelweg de grootste van de
   twee. De tree tekent zichzelf daarbinnen in de gewone volgorde, dus knoppen
   die eroverheen vallen komen vanzelf weer bovenop; de rij expliciete
   `show()`-aanroepen kon weg.
3. **De caption in een eigen regelhoog panel**, met `Text::setOpaque()`. Dat
   wist zijn eigen vak in dezelfde pass. Het panel is nodig omdat
   `Text::calcPaddingSizePix()` dat vak van de *ouder* afleidt: met de page als
   ouder zou het 240 px hoog worden en een strook van bijna het hele scherm
   blanken.

Voor het kruis kwam er `Widget::setDrawable(bool, bIncludeChildren)` bij, de
runtime-tegenhanger van het bestaande `isDrawable()`. Bij een gewoon plaatje
staat het kruis nu niet alleen uit voor touch maar ook uit de tekening, zonder
uit de tree te gaan. De strook waar het staat wordt alleen hertekend als die
zichtbaarheid *verandert* — en als aparte regio, want de bounding box van
plaatje plus kruis zou een brede band meeslepen die niets doet.

### Onderweg gevonden
`Widget::setSize()` gooide zijn `coordTypeSize`-parameter weg. Een widget die in
promillage was aangemaakt las een maat in pixels dus als promillage en werd
minuscuul. Nu wordt het veld gewoon toegekend. Geen bestaande aanroeper leunde
erop: `PageRoot` en `Text` gaven allebei al `Pixels` door aan een widget die dat
ook al was.

### Meten
`showCurrentImage()` logt nu zijn eigen duur in microseconden. De verwachting is
~1 ms voor een frame-stap binnen een sheet en ~10 ms voor de grootste wissel,
tegen ~75 ms daarvoor; dat moet het bord bevestigen.

## 2026-07-27 — Partitietabel zonder OTA

OTA is niet nodig, dus `otadata` en de tweede app-partitie zijn eruit en de app
heet weer `factory`. spiffs is teruggebracht tot 100 KB. Wat dat oplevert:

| | oud | nieuw |
|---|---|---|
| app | 0x140000 (1,25 MB) | 0x3D7000 (3,84 MB) |
| spiffs | 0x170000 (1,44 MB) | 0x19000 (100 KB) |
| otadata + app1 | 0x142000 | — |

De app zelf is 0x66400 (410 KB), dus daar zit nu 90% vrij. spiffs ligt achteraan
zodat de tabel op precies 0x400000 uitkomt. Het gat tussen 0xe000 en 0x10000
blijft: een app-partitie moet op een veelvoud van 0x10000 beginnen.

**Gevolg bij het flashen:** spiffs is verhuisd van 0x290000 naar 0x3E7000, dus
LittleFS vond daar de resten van de oude app1-partitie:

```
lfs.c:1075:error: Corrupted dir pair at {0x0, 0x1}
E esp_littlefs: mount failed, (-84)
I DisplayAdapter: Calibration file not found - forcing recalibration
```

Dat is precies zoals bedoeld - `LITTLEFS.begin()` formatteert dan zelf - maar de
touchkalibratie is er wel mee weg. Eenmalig de hoeken aantikken en hij staat er
weer, op de nieuwe plek.

## 2026-07-27 — Spritesheet-frames: wie bezit de actieve row/col?

### De vraag
De ImageViewer moest een kruis van vier knoppen krijgen om door de frames van een
spritesheet te lopen. Eerste ingeving: row en col erbij in `Image`. Dat klopt
niet, en waarom niet is het hele ontwerp:

**Een `Image` beschrijft de bitmap, niet een verschijning op het scherm.** Hij
heeft ook geen x en y, en om dezelfde reden. Bovendien is hij `constexpr` en
leeft hij in flash: een selectie die verandert kán er niet in staan. En twee
panels die hetzelfde sheet tonen willen elk hun eigen frame.

Dus: `nofRows`/`nofCols` (de indeling van de bitmap) blijven in `Image`, en
`_frameRow`/`_frameCol` (welk frame nú te zien is) horen in `Panel`.

### Wat er is bijgekomen
`Panel::setImageFrame(row, col)` / `clearImageFrame()` / `hasImageFrame()`.
Buiten bereik wordt omgeslagen, zodat doorstappen aan de aanroepkant geen
grenscontrole nodig heeft. `setImage()` laat de framekeuze vallen — die hoorde
bij het vorige plaatje.

De keuze reist mee door de tekenketen: `IDisplay::drawImage` heeft er
`frameRow, frameCol` bij gekregen (negatief = het hele plaatje), en daarmee ook
`AsyncDisplay` en `DisplayCommand`. Die laatste zet ze in zijn ongebruikte
`intB`/`uintA`-velden in plaats van er nieuwe bij te krijgen: het commando-buffer
telt 300 entries, dus elk extra veld kost hier ruim 2 KB DRAM.

### De blit zelf
Een frame is een rechthoek *binnen* de array, dus zijn rijen liggen niet naast
elkaar: elke volgende begint `image.width` pixels verderop. `pushImage` kent geen
source-stride, dus het gaat per scanlijn — één `pushImage` per regel. Dat kost een
`setWindow` (~34 us) per regel in plaats van één voor de hele blit: een 32x32
frame komt op ~1,4 ms waar de bus 0,4 ms nodig heeft.

Bewust niet geoptimaliseerd met een eigen `setWindow` + `pushPixels`: dan verlies
je de viewport-clipping die in `pushImage` zit, en 1 ms voor een sprite is geen
probleem. Als dit ooit wel knelt (veel sprites per frame), is dát de plek.

### In de viewer
Het kruis verschijnt alleen bij een sheet, en wordt dan ook alleen dan
*enabled* — een onzichtbare knop die nog wel luistert, slikt aanrakingen op die
voor iets anders bedoeld waren. De eerste druk op een van de vier zet
`row,col = 0,0` in plaats van weg te stappen van een frame dat nog niet gekozen
was; pas daarna loopt het kruis. Bij een andere afbeelding vervalt de keuze.

## 2026-07-27 — Image-assets in flash: converter-tools en het ImageViewer-voorbeeld

### Wat er nu staat
Een `tools`-map in de projectwortel met Python-gereedschap dat elk door Pillow
leesbaar beeld omzet naar een RGB565-header in flash, plus een nieuw voorbeeld
`CleanGUI_for_TFT_eSPI/examples/ImageViewer` dat die plaatjes met twee
touchknoppen doorbladert.

```
tools/img2rgb565.py      één plaatje omzetten
tools/allImg2rgb565.py   alles omzetten wat veranderd is
tools/venv_bootstrap.py  zorgt dat de venv gebruikt wordt zonder activeren
tools/setup_venv.sh      eenmalig: venv + requirements.txt
tools/README.md          het volledige verhaal
```

### De structuur die dit mogelijk maakt zonder CMake aan te raken
Bronnen staan onder een map `images`; de gegenereerde headers in
`images/_rgb565/`. Het pad onder `images` wordt platgeslagen in de outputnaam
(`level1/characters/speler.png` -> `level1_characters_speler.h`), zodat gelijke
bestandsnamen in verschillende submappen elkaar niet overschrijven.

De example-map staat één keer in `main/CMakeLists.txt`; alles daaronder wordt
met **relatieve** `#include`s bereikt. Er komt dus nooit een CMake-wijziging bij
per plaatje of submap. Bewijs dat dit klopt: het Inputfield-voorbeeld hergebruikt
de plaatjes van Panel via `#include "../Panel/images/_allImages.h"`, en die
`images`-map staat in geen enkele include-pad-lijst.

### Parameters in de bestandsnaam
`-trcolor_RRGGBB` en `-ssh_RxC`, achteraan de naam. Ze blijven in de headernaam
staan (uniek en zelfverklarend) maar verdwijnen uit de C++-namen; daar worden ze
constructor-argumenten. `Image` heeft daarvoor `nofRows`/`nofCols` gekregen
(default 1x1, voorlopig alleen opslag).

**Valkuil bij die constructors:** géén default-argumenten gebruiken. Met
defaults zou `Image(p, w, h, 0x00FF00FF)` stilzwijgend de spritesheet-overload
kiezen in plaats van de transparantie-overload, want een hexliteraal die in een
`int` past *is* een `int` en matcht `int32_t nofRows` exact, terwijl `uint32_t
colTransparent` een conversie vergt. Nu onderscheidt alleen het aantal
argumenten de overloads, en dat kan niet misgaan.

### `_allImages.h` wordt herbouwd uit de headers zelf
Elke gegenereerde header draagt een `// crt-img-meta:`-regel met naam, afmeting,
transparante kleur en sheet-indeling. `_allImages.h` wordt daaruit opgebouwd, dus
er is geen apart administratiebestand dat kan verouderen. Hij bevat ook
`arAllImages[]` / `arAllImageNames[]` / `nofAllImages`, waardoor de ImageViewer
geen enkel plaatje bij naam hoeft te kennen: `.png` erbij, tool draaien, staat
erin.

Het Panel-voorbeeld is op dezelfde leest geschoeid: `crt_TestPanelImages.h` is
gesplitst in `images/gradient.h`, `images/ring.h` en `images/_allImages.h`.

### Twee dingen die bij het testen boven kwamen
- **`venv/bin/python` is een symlink naar de systeem-python.** De eerste
  venv-detectie vergeleek `sys.executable` met `os.path.samefile()` en
  concludeerde dus altijd "we zitten er al in" — waarna `import PIL` faalde.
  `sys.prefix` is wél wat naar de venv verschuift, en is wat `site-packages`
  volgt.
- **De index moet altijd herbouwd worden, niet alleen na een conversie.** Een
  met de hand verwijderde header liet anders een `#include` van een niet-bestaand
  bestand achter in `_allImages.h`, en dat merk je pas veel later als
  compileerfout.

### Incrementeel omzetten
`allImg2rgb565_last_called.txt` (in de images-wortel) bevat epoch-seconden van
het moment waarop de laatste geslaagde run *begon* — niet waarop hij eindigde,
anders valt een bewerking tijdens de run precies in het gat en wordt hij nooit
opgepikt. Er wordt ook omgezet als de header simpelweg ontbreekt, want anders
zou een verse checkout of een weggegooide `_rgb565`-map niets regenereren.

## 2026-07-27 — Touch-polling vrat de helft van de tekentijd

### Aanleiding
Vraag: hoeveel tijd zou het schelen als elke toets een image in flash was, zodat
hij alleen nog geblit hoeft te worden in plaats van getekend? Om dat te kunnen
beantwoorden eerst gemeten waar de tijd van een keyboard-hertekening in gaat -
en daarbij bleek iets heel anders de hoofdrolspeler.

### Antwoord op de blit-vraag
Een volledige hertekening, uitgesplitst per commando (release-build):

| onderdeel | aantal | tijd | gemiddeld |
|---|---|---|---|
| rug (`fillBeveledRect`) | 1 | 16,5 ms | |
| toetsen (`fillBeveledRect`) | 31 | 17,8 ms | 574 us |
| labels (`drawString`) | 31 | 12,3 ms | 397 us |
| **totaal tekenwerk** | 64 | **47,1 ms** | |

Met een image per toets: 25.392 pixels blitten = 10,4 ms, en de labels zitten in
de image. Totaal 27,1 ms, dus **42% minder tekenwerk voor 51 KB flash**.

De rug levert vrijwel niets op (16,5 -> 15,7 ms): een egale vulling zit al op het
busplafond. Blitten helpt alleen waar je nu meer werk doet dan de pixels vragen,
en bij een toets is dat zo: 574 us voor 648 pixels is 886 ns/pixel tegen een
vloer van 409, doordat `fillBeveledRect` zeven losse `fillRect`s met elk een
eigen `setWindow` doet (~34 us overhead per stuk).

Het hele toetsenbord als **een** image zou 15,7 ms zijn - **67% minder** voor
76,8 KB - maar dan zitten de labels vast in het beeld en heeft elke
toetsenbordstand een eigen image nodig.

### De echte vondst: pollTouch
De wandkloktijd van diezelfde hertekening was 65-100 ms terwijl het tekenwerk
constant 43 ms was. Twee verkeerde verklaringen onderweg (queue-overhead; mijn
eigen meetlus die CPU zou stelen) - beide weerlegd door meting. Toen alles apart
geklokt:

```
leegdraaien 90089 us = tekenen 43603 + pollTouch 53495 (2x) + rest -7009
```

**`pollTouch` kostte ~25 ms per aanroep, elke 50 ms.** Losstaand gemeten kostte
een `getTouch()` 14 ms terwijl er niets werd aangeraakt.

### Waar die 14 ms in zat
Met tellers in `validTouch` gemeten: **18 debounce-rondes over 7 aanroepen**, dus
~2,6 rondes per `validTouch` - geen uitschieter. De factor die het deed was dat
`getTouch` `validTouch` **vijf keer** aanroept, onvoorwaardelijk:

| | |
|---|---|
| 5 x validTouch, elk ~2,6 debounce-rondes | ~13 rondes |
| elke ronde `delay(1)` | ~10 ms slapen |
| elke ronde `getTouchRawZ()` van 275 us | ~3,6 ms |

Die 275 us is inherent: de touch-SPI is bit-banged met twee
`delayMicroseconds(1)` per bit. En `delay(1)` slaapt, dus de tekentaak lag stil -
het was geen CPU-verbruik maar blokkade.

De vijf pogingen zijn overigens goed bedoeld: `validTouch` is streng (twee
positiemetingen die binnen `_RAWERR` moeten overeenkomen), en op een resistief
paneel wiebelt een echte vingerdruk. Vijf kansen waarvan er een hoeft te slagen
maakt het geheel ruisbestendig zonder echte drukken te missen. Alleen stopt de
lus nergens vroeg, dus het meest voorkomende geval - niemand raakt aan - betaalt
de volle prijs voor een vraag die na de eerste 275 us al beantwoord was.

### De reparatie
Twee regels in `TFT_eSPI::getTouch()`: een enkele Z-meting vooraf voor het
"geen druk"-geval, en `break` zodra een poging slaagt. Vastgelegd in
**`libs/CleanGUI_for_TFT_eSPI/TOUCH_PERFORMANCE.md`**, want het is een lokale
wijziging in TFT_eSPI die bij een upgrade verdwijnt.

| | voor | na |
|---|---|---|
| `getTouch()`, niets aangeraakt | 14.300 us | **245 us** |
| keyboard-hertekening, wandklok | 65-100 ms | **45,9 ms, stabiel** |

245 us is precies een `getTouchRawZ()`. De hertekening is nu net zo voorspelbaar
als het tekenwerk zelf; alle spreiding kwam van touch-polls die er wel of niet
tussen vielen.

**49% winst zonder een byte flash**, tegenover 42% van het tekenwerk voor 51 KB
aan images. De goedkope knop zat dus ergens anders dan de vraag suggereerde.

### Ook geprobeerd: meerdere commando's per waitAny
`AsyncDisplay` handelde er een per ronde af. Op 10 gezet: 95 -> 85 ms, ruim 10%.
Behouden, maar het was niet de kostenpost waarvoor ik het aanzag.

## 2026-07-26 — getScreenSize klopte niet, en de layout leunde op een race

### Wat er mis was
`crt_TFT_eSPI_DisplayAdapter::getScreenSize()` wisselde breedte en hoogte om bij
rotatie 1 en 3. Maar TFT_eSPI's `width()`/`height()` zijn na `setRotation()` zelf
al gedraaid, dus dat was een omwisseling te veel. Gemeten tijdens het schuiven van
het toetsenbord:

```
getScreenSize=(240,320)   root=(320,240)
```

Precies verwisseld, en daardoor knipte de nieuwe regio-clipping een kwart van de
schermbreedte weg.

### Waarom het jarenlang goed leek te gaan
De adapter kende zijn rotatie pas in `begin()` of `touchCalibrate()`. In de
demo's met `AsyncDisplay` draait `touchCalibrate()` in de AsyncDisplay-taak,
terwijl `PageRoot` de schermmaat opvraagt vanuit de andere taak - en beide taken
worden na hun `vTaskDelay(1000)` op hetzelfde moment wakker. `PageRoot` trof
daardoor het venster waarin `_rotation` al 3 was maar `_tft.setRotation()` nog
niet was uitgevoerd. In dat venster meldt `_tft` nog portret, en dan levert de
dubbele omwisseling toevallig het juiste getal op. **De schermafmeting berustte
dus op een race die telkens dezelfde kant op viel.**

### De reparatie
- `setRotation()` toegevoegd aan `IDisplaySynchrone`, waar `getScreenSize()` ook
  woont. `AsyncDisplay` geeft de rotatie door in zijn **constructor**, dus voordat
  er een taak draait. De adapter onthoudt hem en past hem toe zodra de TFT
  daadwerkelijk is gestart (`_bBegun`).
- `getScreenSize()` leidt de maat af van `TFT_WIDTH`/`TFT_HEIGHT` (in portret
  gedefinieerd) plus de rotatie, en niet meer van `_tft.width()/height()`. Het
  antwoord is daarmee hetzelfde, ongeacht wanneer je het vraagt.

Na de fix: `getScreenSize=(320,240)` en `root=(320,240)`.

### Waarom dit geen enkele demo omgooit
De nieuwe waarde is precies de waarde die de demo's via de gewonnen race al
kregen. Was de race andersom uitgevallen, dan hadden ze (240,320) gekregen en er
zichtbaar verkeerd uitgezien. De fix maakt het deterministisch, niet anders.

`Widget::clipToRoot()` blijft op de rect van de root clippen: de layout is op de
root gebaseerd, dus dat is per definitie consistent - ook als de root ooit kleiner
dan het scherm zou zijn.

## 2026-07-26 — Widgets verplaatsen, en een queue die stilzwijgend werk weggooide

### Vraag
Zou een toetsenbord dat van onderaf in beeld schuift werken door herhaald de
y-positie aan te passen, en zet de widget de vrijgekomen ruimte terug?

**Nee.** `Widget::setLocPos()` herberekent alleen `_locPosPix` en geeft de nieuwe
globale positie door aan de kinderen; het stuurt nul tekencommando's. Tekenen
gebeurt uitsluitend in `show()`/`hide()`, en `hidePanel()` vult de rect waar het
paneel op dat moment staat met zijn eigen `_colBg`. Het bestaande voorbeeld
`crt_TestAsyncDisplay.h:105-115` doet daarom bewust hide -> setLocPos -> show.

### Wat er is gebouwd
- **`setClipRect`/`clearClipRect` in `IDisplayDraw`**, via de viewport, en door de
  commandoqueue heen zodat de clip in de juiste volgorde geldt.
- **`Widget::redrawRegion()`** loopt door de tree en tekent alleen wat de regio
  snijdt. `paint()` is nieuw in `IWidget`: "teken alleen mijzelf", zonder aan
  `_bVisible`/`_bEnabled` te komen.
- **`Widget::moveTo()`** verplaatst, berekent welke stroken vrijkomen (maximaal
  vier; bij een rechte schuif is er een niet-leeg) en laat de tree die herstellen
  vanaf de root. Bewust geen hide/show: dat wist eerst de hele widget (flikker),
  vult met een gokkleur in plaats van te herstellen, en zou de touch-afhandeling
  elke stap aan en uit zetten.
- **`IWidget` kreeg `getParent()`/`setParent()`** - die verwijzing bestond niet,
  dus een widget kon zijn ouder niets vragen.
- Het viewport-conflict opgelost: `drawImage` en `drawStringOpaque` zetten sinds
  eerder vandaag zelf een viewport en deden `resetViewport()`. Met een actieve
  clip gooiden ze die weg. Ze snijden nu door met de clip via
  `applyViewport()`/`restoreViewport()`.

### De "crash" was stilzwijgend commandoverlies
Gemeld: het schuiven liep vast midden in de tweede toetsenrij. Reproduceerbaar
gemaakt door de schuif via dezelfde vlag te triggeren die een aanraking zet.
Resultaat: **682 weggegooide tekencommando's**, types 7 (drawString), 17
(fillBeveledRect) en 19 (**clearClipRect**).

`crt_Queue.h:26` maakt de queue met `bWriteWaitIfQueueFull=false`, dus
`xQueueSend` krijgt timeout 0 en geeft bij een volle queue simpelweg `false`
terug - `AsyncDisplay` negeerde die returnwaarde. Sneuvelt een `clearClipRect`,
dan blijft de clip staan en verandert er daarna niets meer op het scherm. Het
programma draait door; het beeld staat stil. Dat ziet eruit als een crash.

Opgelost: `AsyncDisplay::writeCommand()` wacht tot er plaats is (queue nu met
`bWriteWaitIfQueueFull=true`). Wordt er geschreven vanuit de tekentaak zelf -
touch-afhandeling draait daar - dan zou blokkeren een impasse met zichzelf zijn,
dus maakt hij eerst plaats door zelf een commando uit te voeren. Plus een teller
`getNofQueueFullEvents()`, zodat queuedruk zichtbaar is in plaats van stil.

### Twee eigen misslagen, voor de volledigheid
1. **"Het hangt"** - te snel geconcludeerd uit stilgevallen logregels. Er logde
   daarna simpelweg niets; die stilte bewees niets.
2. **Een optimalisatie die averechts werkte.** Eerder vandaag bleek release 9x
   sneller (284 -> 31 ms/stap) en wees ik `dumpStackHighWaterMarkIfIncreased()`
   in de AsyncDisplay-lus aan als oorzaak. Die aanroep verplaatst naar de timer
   in plaats van per commando: **767 ms/stap**, bijna drie keer trager.
   Teruggedraaid. De werkelijke oorzaak van de wisselende debug-tijden was mijn
   eigen spinlus `while (!write) vTaskDelay(1)` tegen een taak van gelijke
   prioriteit - die gaf 284, 767 en >25000 ms bij identieke code. Met een echt
   blokkerende queue is het stabiel. **Les: laat de queue wachten, ga niet zelf
   pollen.**

### Metingen (15 stappen van 8 pixels, toetsenbord 320x120)
| | debug | release |
|---|---|---|
| per stap | 183 ms | **21 ms** |
| hele schuif | 2757 ms | **319 ms** |

Beter dan de ~38 ms/stap uit het plan, doordat toetsen buiten beeld worden
weggeculld. Zonder buffer blijft een volledige hertekening per stap nodig: de
verschoven inhoud kan niet worden geblit.

### Ontwerppunt: de root tekent niets
`PageRoot` heeft geen eigen kleur, dus zonder een widget die de achtergrond
bezit, is er niets om een vrijgekomen strook mee te herstellen en blijft het
oude beeld staan. Het Inputfield-voorbeeld heeft daarom een `backgroundPanel`
gekregen, als eerste kind toegevoegd en dus als eerste getekend.

### Openstaande bug: getScreenSize klopt niet, en de layout leunt op een race
Gemeten tijdens het schuiven:
```
getScreenSize=(240,320)   root=(320,240)
```
Precies verwisseld. `crt_TFT_eSPI_DisplayAdapter.h:86` wisselt breedte en hoogte
om bij rotatie 1 en 3, terwijl TFT_eSPI's `width()`/`height()` na `setRotation()`
zelf al gedraaid zijn - een dubbele omwisseling.

Dat het tot nu toe goed ging: `PageRoot` vraagt zijn maat op terwijl
`AsyncDisplay` gelijktijdig `touchCalibrate()` draait (beide taken worden na hun
`vTaskDelay(1000)` tegelijk wakker). `PageRoot` treft daardoor het venster waarin
`_rotation` al 3 is maar `_tft.setRotation()` nog niet is uitgevoerd, en dan
levert de dubbele omwisseling toevallig het goede getal op. **De schermafmeting
van dit project berust dus op een race.**

`Widget::clipToRoot()` clipt daarom op de rect van de root en niet op
`getScreenSize()`: de layout is op de root gebaseerd, dus dat is per definitie
consistent. De adapterbug zelf staat nog open - de swap eruit halen zou zonder
meer elke demo op zijn kant zetten, want dan krijgt PageRoot in dat venster
240x320.

## 2026-07-26 — Toetsenbord in toetsstijl, en een verzonken invoerveld

### Aanleiding
Het keyboard had rode afgeronde toetsen op een paarse rug. Gevraagd: de stijl van
een referentie-afbeelding - grijze rechte toetsen met een glanslijntje boven en
links, en donker daarbuiten als schaduw van de kier. Het invoerveld juist
andersom: verzonken in plaats van opstaand.

### Kleuren gemeten in plaats van geschat
PIL was niet beschikbaar, dus de PNG zelf gedecodeerd (zlib + de vijf
PNG-filters). Horizontale doorsnede dwars door een toets:

```
rug 37 | 17, 11 kier | 61 glans | 49 49 50 toetsvlak | 40 | 17, 12 kier
```

en verticaal: kier 12 -> glans 80 -> vlak 49. Dat bevestigde de opbouw: het
donker zit **buiten** het lichte lijntje. De glans is bovenaan sterker (80) dan
links (61), wat klopt met licht van boven.

| | |
|---|---|
| toetsvlak | `0x313131` |
| glans (boven + links) | `0x505050` |
| kier / schaduw (rondom) | `0x0C0C0C` |
| rug van het toetsenbord | `0x222222` |

### Een primitief in plaats van losse lijnen
`IDisplayDraw::fillBeveledRect(topLeft, size, colFg, colHighlight, colShadow,
style)`: schaduwrand rondom, glanslijn daarbinnen. `BevelStyle::Raised` zet die
langs boven en links, `Sunken` langs onder en rechts - want in een uitsparing
valt het licht op de tegenoverliggende binnenwand. Kleuren omwisselen zou niet
werken: de schaduw loopt rondom, alleen de glans verhuist.

Drie kleuren plus een stijl passen exact in de bestaande `uintA/uintB/uintC` en
`intA` van `DisplayCommand`. Dat is de reden voor een primitief in plaats van
zes `fillRect`-aanroepen vanuit `Panel`: bij 31 toetsen scheelt dat ~155
commando's per hertekening, en `CommandBufferSize` is 300.

`Panel` kreeg `setBevel()`, standaard uit. Het keyboard zet hem op elke toets en
op zijn eigen rug (die is zelf ook een toets), hoekradius op 0.

### Wat bijna stukging: opake tekst eet de rand op
Het invoerveld tekent zijn label opaak, en dat gereserveerde vak besloeg het hele
paneel. De blank-strook zou de bevel-lijntjes bij de eerste toetsaanslag hebben
overgeschilderd - rand zichtbaar tot je typt, dan weg. `Text` kreeg daarom een
marge op dat vak, gevuld door `TouchscreenButton` met `Panel::getBevelInsetPix()`
(2 px: schaduwrand plus glanslijn). Omdat `drawStringOpaque` alles op dat vak
clipt, blijft de rand ook heel als de tekst ertegenaan loopt.

## 2026-07-26 — Flikkervrij label vervangen, zonder buffer

### Vraag
Heeft de displaylaag faciliteiten tegen flikker? **Nee, geen enkele.**
`IDisplayDraw` kent alleen directe teken-primitieven; elke widget schildert
onmiddellijk naar de ILI9341 volgens painter's algorithm. `AsyncDisplay` lijkt op
buffering maar serialiseert alleen commando's van meerdere taken. De ILI9341
heeft geen tweede GRAM-pagina. Het enige wat TFT_eSPI biedt is `TFT_eSprite`, en
die wordt nergens gebruikt behalve in `main/spi_benchmark.h`.

Randvoorwaarde was **geen buffer**. Dat sluit sprites en double buffering uit;
dan blijft er één techniek over: *niet wissen wat je gaat overtekenen*.

### Gemeten oorzaak
`Inputfield::updateButtonLabel` deed `Base::show(true)`. Per toetsaanslag, met
`CRT_ASYNCDISPLAY_COMMAND_TIMING` + auto-typer:

```
fillRect     2567-2572 us   (constant: het hele veld, 256x24 = 6144 px)
drawString    335-1355 us   (groeit met de tekstlengte)
```

De 2,57 ms fill komt exact overeen met 6144 px x 409 ns/pixel. Het veld stond dus
**2,9 tot 3,9 ms leeg** per aanslag, in twee losse queue-commando's.

### Na de fix
```
drawStringOpaque  3064-3946 us   (een enkel commando)
```
Totale bustijd gelijk - dezelfde pixels worden geschreven - maar nu *met de tekst
er al in* in plaats van eerst leeg en dan gevuld. De veldbrede wis is weg.
Visueel bevestigd: knippert niet meer.

### Correctie op de eerste aanname
Het plan stelde dat het lege venster "nul per constructie" zou worden. Dat klopt
niet. Bij free fonts vult TFT_eSPI eerst de hele tekstcel en tekent daarna de
glyphs erin (`TFT_eSPI.cpp:5039`). Er blijft dus een wis, maar die beslaat de
tekstcel in plaats van het hele veld, en er zit geen tweede commando achteraan.

### Wat setTextPadding wel en niet doet
Eerst leek `setTextPadding()` onbruikbaar voor free fonts: `padding = 101`
(`TFT_eSPI.cpp:4938`) terwijl de blank-switch alleen 1/2/3 kent. Maar op
`:5041` staat `padding -= 100`, binnen de tak die opake tekst afhandelt. Hij
werkt dus wél - alleen niet voor een lege string, want die tak eist ook dat het
eerste glyph binnen het fontbereik valt. Juist die lege string heeft `hideText()`
nodig. Daarom blankt de adapter zelf, uniform voor beide fonttypes.

### Twee zichtbare artefacten, en wat ze blootlegden
1. **Balk leek te hoog behalve links.** De blank-strook was `fontHeight` (29 px)
   hoog in plaats van de paneelhoogte (24), dus er werd grijs *buiten* het paneel
   geschilderd. Opgelost met `setViewport` om het gereserveerde vak, hetzelfde
   mechanisme als in `drawImage`.
2. **Na de eerste letter verdween het effect links.**
   `applyOffsetForLowerCaseText` zet de x van het label op 0 en gooit daarmee de
   inspringing weg die `setSizeOfParent` aan een MidLeft-label geeft; de tekst
   sprong dus naar de linkerrand en de strook dekte opeens de volle breedte.
   Zo gelaten: die inspringing is `fontHeight`, een verticale maat als
   horizontale marge, en breder dan een letter.

### De onderliggende vondst: het font past niet
```
panel 256x24, fontHeight 29
```
FSB12 heeft een glyph-cel van 29 px in een veld van 24. Transparante tekst
verborg dat door simpelweg buiten de widget te tekenen; zodra tekst zijn eigen
achtergrond meebrengt moet je kiezen tussen 5 px grijs buiten het veld of
clippen. `TouchscreenButton` waarschuwt dit nu expliciet, en het voorbeeld geeft
het invoerveld 140 promillage (33 px) in plaats van 100 (24 px).

### Losse observatie: sdkconfig is grondiger teruggevallen dan gedacht
Naast `CONFIG_FREERTOS_HZ` bleken ook de partitietabel
(`partitions_singleapp.csv` in plaats van de eigen `partitions.csv`) en de
flashgrootte (2MB terwijl de chip er 4 heeft) op defaults te staan. Zonder de
spiffs-partitie op 0x290000 kan `/TouchCalData1` niet worden bewaard, dus vroeg
het bord bij elke boot om touchkalibratie. Na het herstellen van beide bleek de
oude kalibratiedata er nog gewoon te staan: `[226, 3403, 319, 3413, 7]`.
**Overweeg een `sdkconfig.defaults` in de repo** - `sdkconfig` staat in
.gitignore, dus deze reeks gaat zich anders herhalen.

## 2026-07-26 — Images in CleanGUI: bufferloos 16bpp blitten uit flash

### Doel
Een `Panel` kan bij constructie een image meekrijgen en tekent dan die image in
plaats van een gekleurde rechthoek. Rechtstreeks 16bpp blitten uit `const`
geheugen, zónder tussenbuffer; bij transparantie de runs van niet-transparante
pixels per blok versturen. CleanGUI blijft display-adapter-onafhankelijk.

### Uitkomst van het vooronderzoek: géén TFT-sprite nodig
TFT_eSPI heeft alles al in huis:
- `pushImage(x,y,w,h,uint16_t*)` (TFT_eSPI.cpp:1361) schrijft een ongekapt beeld
  in één `pushPixels(data, w*h)` — nul kopieën, van .rodata rechtstreeks de
  SPI-FIFO in.
- De transparante overload (TFT_eSPI.cpp:1391) verzamelt per beeldregel runs van
  niet-transparante pixels en schrijft elke run weg met één `setWindow()` +
  `pushPixels()`. Precies de gewenste per-pixel/per-blok-splitsing.
Een sprite ertussen zou alleen een kopie en `2*w*h` bytes DRAM toevoegen.

Twee bevindingen die het ontwerp bepaalden:
- **De `const`-overload is nét niet de snelste.** Regel 1450 kopieert eerst regel
  voor regel naar een stack-lijnbuffer en pusht dán. Die splitsing bestaat voor
  AVR, waar PROGMEM een aparte adresruimte is. Op de ESP32 is flash
  memory-mapped en `pushImage` leest alleen — een `const_cast` in de adapter is
  hier dus veilig en levert de echte bufferloze weg op.
- **Byte-volgorde kost niets.** `pushSwapBytePixels`
  (Processors/TFT_eSPI_ESP32.c:288) draait de bytes om terwijl de vorige
  512-bits transfer nog loopt: 16 shuffles tegen 12,8 µs bustijd, volledig
  verstopt. Data dus in host-volgorde houden en `setSwapBytes(true)` zetten
  (netjes terugzetten na afloop, want het is globale state op `_tft`).

### Wat is toegevoegd
- **`libs/CleanGUI/src/crt_Image.h`** (nieuw): `enum class PixelFormat`,
  `class Image` (opaque handle: `pData`, `width`, `height`, `format`,
  `bHasTransparency`, `colTransparent` in ARGB), plus de vrije functie
  `calcAlignedTopLeftPix()`.
  CleanGUI raakt de pixels nooit aan; de format-tag laat een andere adapter
  weigeren of converteren. Bewust géén abstracte `IImage` met `getPixel()` —
  dat zou elke pixel virtueel maken. `Image` heeft `constexpr` constructors, dus
  een image kan volledig in flash staan.
- **`IDisplayDraw`** kreeg er één functie bij:
  `drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment)`.
  Semantiek: plaats de image binnen de rechthoek volgens `alignment` en **clip**
  hem daarop. De uitlijnrekensom zit in `calcAlignedTopLeftPix()` in CleanGUI —
  gedeeld, niet per adapter gekopieerd. Er is bewust géén aparte clip-rechthoek
  in de signatuur: de doelrechthoek *is* de cliprechthoek, waardoor het commando
  in de bestaande `vecA`/`vecB`/`intA`-velden van `DisplayCommand` past.
- **`Panel`**: extra ctor-overload met `const Image&`, plus `setImage()`/
  `getImage()`. De image wordt uitgelijnd met de al bestaande `Alignment` van de
  widget (die voor Panel tot nu toe ongebruikt was).
- **Drie implementors bijgewerkt** (dat is de complete lijst):
  `TFT_eSPI_DisplayAdapter`, `AsyncDisplay`, `DisplayCommand`
  (`drawImage=15` plus één `const Image*`-veld; lifetime-afspraak identiek aan
  die van `str`, en voor const statics in flash automatisch waar).

### Clipping: via de viewport van TFT_eSPI
`setViewport(x, y, w, h, false /*vpDatum*/)` zet de cliprechthoek zonder de
oorsprong te verschuiven, waarna de `PI_CLIP`-macro binnenin élke `pushImage`
het kappen doet. Daarna `resetViewport()`. Geen eigen clip-rekenwerk, en het is
exact hetzelfde pad dat de sprite-code van de library gebruikt.

### Achtergrond niet vooraf vullen
`Panel::needsBackground()` slaat de `fillRect`/`fillSmoothRoundRect` over als er
een image is die de panel volledig bedekt (verspilde bustijd) én altijd als de
image transparantie heeft — daar zijn juist de pixels van de ouder eronder wat
zichtbaar moet blijven. Gevolg: een dekkende image die de panel vult onderdrukt
ook de afgeronde hoeken; een rechthoekige blit kan die nu eenmaal niet honoreren.

### Meegenomen foutje
De copy-ctor en `operator=` van `DisplayCommand` kopieerden `intB` niet — het
veld waar `drawString` de alignment in stopt. Het viel niet op omdat
`Queue::write` via FreeRTOS een rauwe memcpy doet. Bij het toevoegen van
`pImage` alsnog rechtgezet.

### Voorbeeld
`libs/CleanGUI_for_TFT_eSPI/examples/Panel` uitgebreid met drie image-panels:
- `crt_TestPanelImages.h` (gegenereerd): `imageGradient` (64x48, dekkend, met
  witte rand en zwarte diagonaal) en `imageRing` (64x64, schietschijf met
  magenta 0x00FF00FF als transparante kleur). Samen 14.336 bytes in .rodata.
- panel groter dan de image, `MidMid` → gecentreerd met blauwe rand eromheen;
- panel kleiner dan de image, `BottomRight` → zichtbaar gekapt;
- transparante ring als kind van een groen achtergrondpaneel → het groen blijft
  door de transparante delen heen zichtbaar.

### Verificatie
`idf.py build` compileert schoon met `#include <Panel.ino>` in main.cpp.
`nm` bevestigt dat beide pixelarrays én beide `Image`-objecten op 0x3F40xxxx
liggen (flash-gemapte .rodata): nul bytes DRAM.
Nog **niet** op de CYD getest — `/dev/ttyUSB0` bestond niet meer na de
WSL-herstart (usbipd opnieuw attachen).

### Meting: alles is busgelimiteerd, behalve de transparante blit
Het voorbeeld tekent nu zo snel als de bus toelaat en rapporteert elke 2 s een
exponentieel voortschrijdend gemiddelde (`ema += (measured - ema) / 16`, geen
history-buffer). Elk paneel wordt apart geklokt. Resultaat, rotsvast:

```
17.85 fps | clear 31.44 ms
  show (us): red 2685 | centered 3833 | clipped 1021 | backdrop 12745 | ring 3145
```

Referentiesnelheid uit de fullscreen clear zelf: 76.800 px / 31.440 us =
**409 ns/pixel**. Daarmee klopt de boekhouding tot op 0,5%:

| onderdeel | gepushte pixels | bus-verwacht | gemeten | premie |
|---|---|---|---|---|
| clear 320x240 | 76.800 | (referentie) | 31,44 ms | 1,00 |
| rood paneel 80x80 | 6.400 | 2,62 ms | 2,68 ms | 1,02 |
| centered: fill 84x68 + image 64x48 | 8.784 | 3,59 ms | 3,82 ms | 1,06 |
| clipped: 40x40 uit de image | 1.600 | 0,65 ms | 1,02 ms | 1,56 |
| backdrop fillSmoothRoundRect 230x130 | 29.900 | 12,23 ms | 12,72 ms | 1,04 |
| ring, transparant | 1.592 | 0,65 ms | 3,06 ms | **4,70** |

Totaal 54,74 ms + 1 ms `vTaskDelay` = 55,74 ms -> 17,94 fps (gemeten 17,85).

**`fillSmoothRoundRect` is niet traag.** Hij zit 4% boven het busplafond, dus hij
vult het interieur gewoon met blokken en anti-aliast alleen de hoekbogen. De
eerdere verdenking (per-pixel anti-aliasing, 581 ns/pixel) klopt niet.

**De enige echte premie zit op transparantie: 4,7x.** Niet per pixel maar per
run: de ring heeft 1.592 niet-transparante pixels verdeeld over **200 runs**, en
elke run kost een `setWindow`. 2,4 ms extra / 200 = ~12 us per setWindow. Een
transparante image is dus goedkoop zolang de runs lang zijn, en duur bij veel
losse eilandjes. Dat is het echte ontwerpcriterium, niet het pixelaantal.

### Correctie op een vuistregel in CLAUDE.md
De regel "`pushImage` haalt 12,4 ns/pixel, `drawPixel` 581 ns/pixel" komt uit de
tabel in de entry van 2026-07-20, waar die getallen `spr.pushImage()` en
`spr.drawPixel()` heten: **sprite-operaties in geheugen**, geen SPI-uitvoer.
CLAUDE.md heeft de `spr.`-prefix laten vallen, waardoor het leest als
display-doorvoer. 12,4 ns/pixel = 161 MB/s en is over een 40 MHz-bus fysiek
onmogelijk. Het busplafond is ~400 ns/pixel (2 bytes bij 40 MHz plus overhead);
diezelfde entry zegt dat zelf ook al: 64x64 pushen naar het scherm kost 1645 us,
oftewel 402 ns/pixel.

### Helpt een release-build? Nauwelijks -- en dat bevestigt de diagnose
Gemeten met `CONFIG_COMPILER_OPTIMIZATION_PERF` (-O2) + assertions uit,
tegenover de debug-build (`CONFIG_COMPILER_OPTIMIZATION_DEBUG`):

| | debug | release | verschil |
|---|---|---|---|
| clear 320x240 | 31,44 ms | 31,30 ms | -0,4% |
| rood paneel | 2680 us | 2674 us | -0,2% |
| backdrop roundrect | 12724 us | 12717 us | -0,1% |
| centered + image | 3816 us | 3753 us | -1,6% |
| clipped image | 1021 us | 922 us | **-9,7%** |
| transparante ring | 3059 us | 2645 us | **-13,5%** |
| totaal | 17,85 fps | 18,17 fps | +1,8% |

De drie busgelimiteerde posten bewegen niet -- daar wacht de CPU op de SPI.
Precies de twee posten met per-run CPU-werk winnen 10-14%. Onafhankelijke
bevestiging dat de 4,7x premie op de transparante blit de setWindow-aanroepen
zijn en niet iets aan de bus.

Let op: `CRT_HIGH_WATERMARK_INCREASE_LOGGING` (de stack-dumps, ~0,25 ms per
lus-iteratie) speelt in dit voorbeeld geen rol -- `TestPanel::main` roept
`dumpStackHighWaterMarkIfIncreased()` niet aan. Waar dat wel gebeurt is het een
heel ander verhaal, met name `crt_AsyncDisplay.h:51`: daar gaat hij af per
verwerkt teken-commando.

sdkconfig staat weer op debug; 1,8% weegt niet op tegen asserts tijdens de
ontwikkeling.

### De echte hefboom voor de framerate
De fullscreen clear is 31,4 van de 55,7 ms (56%) en wordt daarna grotendeels
weer overtekend door de panelen. Niet elk frame wissen -- of alleen wissen wat
geen paneel bedekt -- verdubbelt de framerate zowat. Aan de panelen zelf valt
weinig te halen: die zitten al op de bus vast.

### Losse observatie
`sdkconfig` (staat in .gitignore) was teruggevallen op defaults, met
`CONFIG_FREERTOS_HZ=100`; arduino-esp32 eist 1000. Handmatig teruggezet, anders
komt de build niet eens door cmake heen.

## 2026-07-21 — _bDrawable/isDrawable() toegevoegd aan CleanGUI-widgets

### Aanleiding
Sommige widgets moeten wél meedoen in de widget-tree en touch-afhandeling, maar
níets tekenen — typisch een onzichtbare TouchscreenButton die alleen een
touch-gebied afvangt.

### Wat is toegevoegd
- `IWidget` (`crt_IWidget.h`): `virtual bool isDrawable() = 0;`
- `Widget` (`internals/crt_Widget.h`): member `_bDrawable` (naast `_bVisible`/
  `_bEnabled`), gezet via nieuwe trailing ctor-parameter `bool bDrawable = true`,
  plus de `isDrawable()`-implementatie.
- `Panel`, `Text`, `TouchscreenButton`: ctor kreeg dezelfde trailing parameter
  (default `true`, dus geen enkele bestaande call site hoefde aangepast).
  TouchscreenButton geeft `bDrawable` door aan zowel de Panel-basis als het
  Text-kind (label van een onzichtbare knop wordt ook niet getekend).
- Guard `if (!isDrawable()) return;` vooraan in de functies die daadwerkelijk
  display-commando's queuen: `Panel::showPanel/hidePanel`, `Text::showText/hideText`.

### Semantiek
`show()/hide()` blijven gewoon werken voor niet-tekenbare widgets: `_bVisible`
en `_bEnabled` (en dus touch) worden normaal bijgehouden; alleen de
display-aanroepen worden overgeslagen. PageRoot/ButtonGroup/Keyboard/Inputfield
tekenen zelf niets en hadden geen aanpassing nodig.

### Verificatie
`idf.py build` compileert schoon (Inputfield-demo-app, gebruikt alle betrokken
widgettypes). Gedrag ongewijzigd voor bestaande code omdat de default `true` is.

## 2026-07-21 — Semantiek van Widget::show/hide in CleanGUI

### Aanleiding
Analyse van de flikker bij het typen in de Inputfield-demo. Om de fix goed te
kiezen moest eerst precies duidelijk zijn wat `show(true)`/`hide(true)` doen.

### Call-tree van een knop (tsButtonQ: Panel met afgeronde hoeken + Text-kind "q")
```
show(true)                          [Panel::show, crt_Panel.h:519]
├─ showPanel()                      -> display->fillSmoothRoundRect(...)  (queue)
└─ Widget::show(true)               [crt_Widget.h:275]
   ├─ _bVisible = true
   ├─ enable(false)                 -> bPressed=false, _bEnabled=true (touch AAN)
   └─ per kind: child->show(true)
      └─ text.showText()            -> display->drawString("q", ...)      (queue)

hide(true)                          [Panel::hide, crt_Panel.h:526]
├─ hidePanel()                      -> display->fillRect(rect, _colBg)    (queue)
└─ Widget::hide(true)
   ├─ _bVisible = false
   ├─ disable(false)                -> _bEnabled=false (touch UIT)
   └─ per kind: child->hide(true)
      └─ text.hideText()            -> LEGE STUB, tekent niets
```
De display-aanroepen zijn queue-writes naar AsyncDisplay; het echte tekenen
gebeurt later in de AsyncDisplay-taak (DisplayCommand::execute -> DisplayAdapter
-> _tft -> SPI -> ILI9341).

### Betekenis
- **`show(true)` = "schilder mij nú, en zet mij aan."** Achtergrond eerst, dan
  kinderen (painter's algorithm), plus `_bVisible=true` en touch inschakelen.
- **`hide(true)` = "schilder mij wég, en zet mij uit."** Verbergen bestaat niet
  op de ILI9341 - het is de eigen rect overschilderen met `_colBg`, plus
  `_bVisible=false` en touch uitschakelen.

### Drie asymmetrieën om te onthouden
1. **Tekst wordt nooit expliciet gewist.** `Text::hideText()` (crt_Text.h:847) is
   leeg; tekst verdwijnt alleen doordat de panel-fillRect eroverheen valt. Een
   Text zonder Panel eronder kan dus niet verborgen worden.
2. **Show/hide zijn imperatieve schilderopdrachten, geen state-wissels.** Er is
   geen renderloop die `_bVisible` leest; de recursie in Widget::show tekent
   kinderen onvoorwaardelijk. Daarom plakt een eenmalige `hide()` niet: de
   eerstvolgende `show(true)` op de ouder tekent het kind gewoon weer.
3. **Zichtbaarheid en touch zijn gescheiden velden.** Touch-afhandeling checkt
   alleen `_bEnabled` (crt_TouchscreenButton.h:176), niet `_bVisible`. "Niet
   tekenen maar wel touch ontvangen" is dus technisch mogelijk - relevant voor
   de onzichtbare touch-catchers (tsOutsideInputfieldButton/-KeyboardButton).

### Verwante bevinding (zelfde sessie)
De flikker bij typen komt doordat `Inputfield::updateButtonLabel` de hele
subtree hertekent, waarbij eerst `tsOutsideInputfieldButton` (100% vlak,
colFg==colBg==zwart) wordt geschilderd en daaroverheen de echte knop. Let op:
de vergelijkbare `tsOutsideKeyboardButton` is NIET puur onzichtbaar - die
schildert de paarse achtergrond van het keyboard en wist het vlak bij hide
(commentaar op crt_TouchscreenKeyboard.h:73: "displays the back of the
keyboard"). `bInvertedArea` is dus geen betrouwbaar signaal voor "niet tekenen";
een fix moet die twee gevallen uit elkaar houden. Nog niets gewijzigd.

---

## 2026-07-20 — Intern RAM versus PSRAM, en andere ESP-chips

### Aanleiding
Zijterzijde bij de sprite-metingen: het RAM op dit bord is de beperkende factor
(204 KB vrij, maar grootste aaneengesloten blok 110.592 bytes). Vraag: zijn er
ESP-chips met meer RAM?

### Intern SRAM per chip
| chip | intern SRAM |
|---|---|
| ESP32 (de CYD) | 520 KB |
| ESP32-S2 | 320 KB |
| ESP32-C3 | 400 KB |
| ESP32-S3 | 512 KB |
| ESP32-C6 | 512 KB |
| ESP32-P4 | 768 KB |

**Overstappen van chip levert dus vrijwel niets op.** Een paar honderd KB, waarvan
een flink deel naar stacks, WiFi/BT-buffers en fragmentatie gaat. De echte
oplossing is PSRAM.

### Het verschil tussen intern SRAM en PSRAM
**Intern SRAM** zit op de die zelf, direct aan de geheugenbus van de CPU. Toegang
kost effectief een paar klokcycli, er komt geen cache aan te pas, en het is
DMA-capable. Het is duur in siliciumoppervlak — vandaar dat het bij een paar
honderd KB blijft, ongeacht welke chip je pakt.

**PSRAM** is een aparte geheugenchip, in dezelfde package of ernaast, verbonden
via een SPI-bus (quad of octal). De CPU adresseert hem niet rechtstreeks: het
gaat via hetzelfde memory-mapped cache-mechanisme als flash. Een cache-hit is
snel, een cache-miss is een SPI-transactie. Dus: megabytes in plaats van
kilobytes, maar trager en afhankelijk van cachegedrag.

Vuistregel: intern SRAM is écht werkgeheugen; PSRAM gedraagt zich meer als een
zeer snelle, memory-mapped schijf.

Voor dit probleem telt vooral dat PSRAM als **één groot aaneengesloten blok**
beschikbaar komt — precies waar het hier op stukloopt.

### Praktische keuze
**ESP32-S3 met octal PSRAM** (8 MB). Octal is fors sneller dan de quad-PSRAM van
de klassieke ESP32, die de bus bovendien met flash deelt. TFT_eSPI heeft er al
een processor-header voor: `libs/TFT_eSPI/Processors/TFT_eSPI_ESP32_S3.h`.
`callocSprite` gebruikt automatisch `ps_calloc` zodra `CONFIG_SPIRAM_SUPPORT`
aan staat en er PSRAM gevonden wordt (`Extensions/Sprite.cpp:165`).

### Twee kanttekeningen
- **PSRAM en DMA gaan niet samen** in TFT_eSPI. `Sprite.cpp:165` valt bewust terug
  op intern RAM zodra `_tft->DMA_Enabled` waar is. Logisch: de DMA-engine leest
  alleen internal DRAM (zie de DMA-uitleg in de entry hieronder).
- **De CYD is een ESP32-WROOM-32 zonder PSRAM.** Dit betekent dus een ánder bord,
  geen configuratie-instelling.

---

## 2026-07-20 — Sprites, flash-assets en de DMA-omslag (CYD / ILI9341)

### Aanleiding
Vervolg op de DMA-sessie hieronder. Vraag: hoeveel levert DMA op als de
renderloop het hele beeld opnieuw tekent, en wat is de rol van sprites bij
images die in flash staan?

### 0. Wat DMA hier precies doet
Zonder DMA **ís de CPU de datapomp**: `pushPixels()` schrijft 16 woorden (64 bytes)
in de SPI-FIFO-registers `SPI_W0..W15`, wacht tot `SPI_USR` klaar is, en herhaalt.
Fullscreen = 2400 van die rondjes. De gaatjes tussen de bijvullingen zijn precies
waarom we 36 MHz effectief maten in plaats van 40.

Met DMA krijgt een state machine op de geheugenbus één descriptor (adres + lengte)
en vult die de FIFO zelfstandig bij. De CPU zet het op en is vrij. Twee
hardware-eisen volgen daaruit: de engine kan **alleen internal DRAM** lezen (niet
door de flash-cache heen — vandaar dat flash-assets eerst naar een sprite moeten),
en de buffer moet **onaangeroerd blijven** tot de transfer klaar is — dát is de
echte reden voor double buffering, geen software-conventie.

Softwarematig gebruikt TFT_eSPI de peripheral via twee paden tegelijk: normaal
direct registers rammen, en daarnaast de IDF `spi_master`-driver die `initDMA()`
opzet (`spi_bus_initialize` + `spi_bus_add_device` → `dmaHAL`). `pushImageDMA()`
doet dan: `dmaWait()` → `setAddrWindow()` via de registerroute (commando's zijn te
klein voor DMA) → `spi_device_queue_trans()`, dat meteen terugkeert. `queue_size`
is 1, dus er loopt er hooguit één tegelijk.

Daaruit volgt alles wat we maten: +10% ruwe doorvoer (geen gaatjes meer), ~40 µs
vaste overhead per push (waardoor DMA onder 32x32 juist verliest), en een vrij
CPU-venster ter grootte van de transfer (waardoor de DMA-kolommen vlak blijven).
De engine is een domme kopieermachine die niets kan omdraaien — sprites slaan
16bpp al byte-swapped op, dus `setSwapBytes(false)` is verplicht.

### 1. Fullscreen redraw is transfer-bound op ~32 fps
Fullscreen in stroken van 80px, bij oplopende tekenbelasting:
```
tekenwerk    zonder DMA          met DMA            winst
0,86 ms      35,20 ms (28,4)     30,98 ms (32,3)    1,14x
2,10 ms      36,43 ms (27,4)     30,99 ms (32,3)    1,18x
4,06 ms      38,38 ms (26,1)     31,01 ms (32,2)    1,24x
7,88 ms      42,21 ms (23,7)     31,06 ms (32,2)    1,36x
```
De DMA-kolom is **vlak**: 30,98 / 30,99 / 31,01 / 31,06. Tekenwerk vertienvoudigt,
frametijd stijgt 0,08 ms. Volledig verstopt achter de transfer.

153.600 bytes bij 4,98 MB/s = 30,8 ms. Dat is 32 fps en dat is een **hard
plafond** — geen enkele optimalisatie komt daaroverheen. DMA zet je erop en houdt
je erop. Theoretisch maximum is 2x, precies wanneer tekenen even lang duurt als
de transfer. Daarboven word je render-bound.

### 2. Twee modellen, en alleen één ervan heeft double buffering nodig
Dit onderscheid bleek de kern van de verwarring:

- **Immediate mode** (elk frame pixels *genereren* in een buffer): double
  buffering verplicht, anders schrijf je in wat de DMA-engine uitleest.
  Kost een tweede buffer. Zo werkt SpriteRotatingCube.
- **Statische sprites** (assets één keer bouwen, daarna alleen *pushen*):
  niemand schrijft in de sprite, dus **geen tweede buffer, nul extra bytes**.
  De overlap is dan met gamelogica in plaats van met tekenwerk.

Let op: het patroon "asset uit flash naar sprite blitten" is een *schrijf*
actie, dus dat valt onder immediate mode en heeft wél double buffering nodig.

### 3. Bij kleine sprites is DMA verlies
40 sprites per frame, statische assets:
```
sprite   zonder DMA   met DMA    100us logica/entity: zonder → met
16x16    4,86 ms      6,39 ms    8,89 → 6,13 ms  (1,45x)
32x32   18,57 ms     18,67 ms   22,56 → 18,67 ms (1,21x)
64x64   73,22 ms     67,84 ms   77,25 → 67,83 ms (1,14x)
```
Vaste kosten van `spi_device_queue_trans` + `dmaWait` zijn ~40 µs per push. Bij
512 bytes (16x16) is dat meer dan je aan de transfer wint. **Omslagpunt ligt rond
32x32.**

Met gamelogica erbij: de DMA-kolom blijft weer vlak (6,39→6,13, 18,67→18,67,
67,84→67,83). 4 ms logica per frame volledig geabsorbeerd.

### 4. Flash-assets: de kopie is niet je probleem
```
sprite   uit flash              uit RAM      verschil
16x16     7,5 us                 7,2 us       4%
32x32    17,7 us                17,4 us       2%
64x64    50,9 us (161 MB/s)     49,9 us       2%
```
De flash-cache doet zijn werk. **Assets naar RAM kopiëren om sneller te blitten
heeft geen zin.** En 64x64 blitten kost 51 µs tegen 1645 µs om het naar het
scherm te sturen — de kopie is **3%** van de transfer.

### 5. Vermijd per-pixel werk, niet sprites
```
spr.drawPixel()             581 ns/pixel
rauwe store via getPointer  121 ns/pixel   (4,8x sneller)
spr.pushImage() (bulk)     12,4 ns/pixel   (47x sneller dan drawPixel)
```
`drawPixel` doet per pixel een `_created`-check, `_vpOoB`-check, datum-optelling,
vier range-vergelijkingen, een bpp-branchketen en een byte-swap rond één store.

Maar `pushImage` is 10x sneller dan een handgeschreven pixelloop, want het
kopieert rijgewijs in bulk. De les is dus **niet** "vermijd sprites" maar
"vermijd per-pixel werk".

Volledige pipeline (blit uit flash + push, 40 assets/frame) wijkt nauwelijks af
van alleen pushen: 76,09 vs 73,22 ms bij 64x64. Blitten is 4% van het frame.

### Wat een sprite toevoegt t.o.v. boing_ball's scanline-buffers
Een sprite ís een RAM-buffer plus teken-API; `getPointer()` geeft dezelfde
`uint16_t*`. Op de transfer: nul verschil, DMA ziet alleen een pointer. Het
verschil is **geheugenstrategie**: boing_ball streamt 320x240 met 1,2 KB
(twee scanlines), een sprite zou 153 KB kosten. Je betaalt RAM voor willekeurige
toegang, compositie en de teken-primitieven.

### Praktische vuistregels
- Sprites ≥32x32 én noemenswaardige CPU-tijd per frame → DMA loont.
- Veel kleine sprites, weinig logica → DMA overslaan.
- Fullscreen redraw → je zit vast aan 32 fps, ongeacht wat je doet.
- Blit met `pushImage`, nooit met een `drawPixel`-loop.

### Bestanden
`main/spi_benchmark.h` uitgebreid met `benchFullScreen()`, `benchRenderOnly()`,
`benchGameSprites()`, `benchBlit()`, `benchPerPixel()`, `benchAssetPipeline()`.
Nieuw: `main/flash_asset.h` (gegenereerde 64x64 RGB565 asset in `.rodata`).

`main.cpp` draait nu SpriteRotatingCube (**151 fps**; Bodmer noteert voor ESP32
40 MHz: 127 fps zonder DMA, 145 met — wij zitten hoger, vermoedelijk door de
HSPI IOMUX-pinnen). Twee dingen waren nodig om dat voorbeeld te bouwen:
- De map heet `DMA test` **mét spatie**. Die kan niet in
  `COMPONENT_ADD_INCLUDEDIRS` — de legacy `register_component()` splitst de lijst
  alsnog op spaties. Opgelost met een relatief pad in de `#include`.
- `drawCube()` forward declaren; de `.ino` roept hem aan vóór de definitie.

`Flash_Jpg_DMA` is **niet** gedraaid: vereist TJpg_Decoder, dat niet in de repo
zit. Bovendien gebruikt dat voorbeeld helemaal geen sprites — het decodeert JPEG
uit flash naar 16x16 tegels in kale arrays. `Bouncy_Circles` past hier sowieso
niet: twee sprites van 240x160 = 153.600 bytes tegen een vrij blok van 110.592.

---

## 2026-07-20 — DMA gemeten (CYD / ILI9341)

### Aanleiding
Vervolg op de SPI-sessie hieronder: daar stond DMA nog als "grootste resterende
snelheidswinst" genoteerd. Nu daadwerkelijk aangezet en gemeten.

### Wat er nodig was
Niets aan de configuratie. `ESP32_DMA` is in `Processors/TFT_eSPI_ESP32.h`
al gedefinieerd voor elke niet-parallelle bus; de DMA-code was dus altijd al
meegecompileerd. Ontbrak alleen de runtime-aanroep `tft.initDMA()` plus
`pushImageDMA()` in plaats van `pushSprite()`.

`initDMA()` doet zelf `spi_bus_initialize()` op dezelfde HSPI-host die Arduino's
SPI-laag al gebruikt. Dat botst in de praktijk niet — geen fout, geen assert.

Let op: `pushImageDMA()` slikt de sprite-pointer rauw. Sprite-pixels staan al in
de byte-volgorde van het display, dus `tft.setSwapBytes(false)` vóór het pushen.

### Meetresultaten (40 MHz, HSPI, `main/spi_benchmark.h`)
```
--- blocking pushSprite ---
240x 30 :   3.200 ms/push,  4.50 MB/s,  36.0 MHz effectief
240x160 :  17.007 ms/push,  4.52 MB/s,  36.1 MHz effectief
--- pushImageDMA ---
240x 30 :   2.939 ms/push,  4.90 MB/s,  39.2 MHz effectief
240x160 :  15.425 ms/push,  4.98 MB/s,  39.8 MHz effectief

--- render+push, 240x80 ---
zonder DMA (1 sprite) :  8.947 ms/frame, 111.8 fps
met DMA (2 sprites)   :  7.765 ms/frame, 128.8 fps
```

### Conclusie
**Ruwe doorvoer: +10%.** Van 36 naar ~40 MHz effectief. Dat is precies wat je
kunt verwachten: DMA haalt de per-pixel CPU-overhead uit de transfer, waardoor
de bus vrijwel de volle 40 MHz haalt. Meer zit er niet in — 40 MHz ís het
plafond, en blocking pushSprite zat daar al op 90%.

**Render+push double buffered: +15% (112 → 129 fps).** De rekensom laat zien
waarom niet meer: een 240x80 sprite kost 38.400 bytes, bij 4,98 MB/s is dat
7,71 ms. Gemeten werd 7,765 ms/frame. Het tekenwerk is dus **volledig** achter
de transfer weggewerkt — de overlap is perfect, er valt niets meer te winnen.

De winst van double buffering schaalt dus met hoeveel je per frame tekent, tot
maximaal de transfertijd. Bij dit lichte tekenwerk (fillSprite + 8 cirkels,
~1,2 ms) is dat weinig. Bij zwaarder tekenwerk loopt het op naar bijna 2x, maar
nooit verder: zodra het tekenen langer duurt dan de transfer is de CPU weer de
bottleneck.

### Kosten
Double buffering kost een tweede sprite. Grootste aaneengesloten blok is 110.592
bytes, dus twee 16bpp-sprites samen mogen niet boven ~110 KB uitkomen: 240x80
past (2x 38,4 KB), 240x160 niet (2x 76,8 KB). Fullscreen 240x320 past sowieso
niet, ook niet enkel — dat blijft 8bpp-met-palette of stroken.

### Status
`main/spi_benchmark.h` bevat nu ook `benchOneSizeDma()` en
`benchDoubleBuffer()`. De aanroep in `main.cpp` staat weer uitgecommentarieerd.
De app zelf is **niet** omgebouwd naar DMA — dat is een aparte ingreep.

---

## 2026-07-20 — SPI-instellingen display (CYD / ILI9341)

### Aanleiding
Vraag over beschikbaar RAM voor sprites, uitmondend in een analyse van de
display-interface. Daarbij bleek de SPI-configuratie suboptimaal.

### Context: geheugen (gemeten, niet geschat)
Target is een **plain ESP32 zonder PSRAM** (`CONFIG_SPIRAM is not set`).

Op het bord gemeten na volledige init:
```
free heap 204144, largest block 110592
```

**Die twee getallen zijn niet inwisselbaar en dat is hier het hele punt.** Er is
204 KB vrij, maar het grootste aaneengesloten blok is 110.592 bytes. Een sprite
vereist één aaneengesloten allocatie, dus het tweede getal is het plafond.

| sprite | bytes | past? |
|---|---|---|
| fullscreen 16 bpp | 153.602 | **nee** — bevestigd, `createSprite` faalt |
| fullscreen 8 bpp + palette | 76.802 | ja |
| fullscreen 4 bpp | 38.402 | ja |
| 240x230 16 bpp (praktisch max) | 110.402 | net |
| band 240x30 16 bpp | 14.402 | ruim |

Merk op: in TFT_eSPI *is* een sprite de framebuffer — er is geen apart
framebuffer-mechanisme. Direct naar de TFT tekenen kost 0 RAM. De ILI9341 heeft
172.800 bytes GRAM aan boord, maar dat is precies één frame en fungeert als
frontbuffer (continu uitgescand op ~70 Hz); het is geen bruikbaar scratchgeheugen.

### Wijzigingen in `libs/TFT_eSPI/User_Setup.h`

**1. `SPI_FREQUENCY`: 27 MHz → 40 MHz**

De ESP32 deelt de 80 MHz APB-klok met een geheel getal. 27 MHz bestaat niet als
deler en leverde in de praktijk 80/3 = 26,67 MHz. 40 MHz is exact 80/2.
Effectief ~50% meer bandbreedte.

**2. `USE_HSPI_PORT` ingeschakeld**

Dit is *geen* pinwijziging — de pinconfiguratie (MISO 12, MOSI 13, SCLK 14,
CS 15) blijft ongemoeid. Het bepaalt welke SPI-controller die pinnen aanstuurt.

Op dit bord (CYD, ESP32-2432S028R) zijn 12/13/14/15 de IOMUX-pinnen van HSPI;
de bedrading is dus voor HSPI ontworpen. Zonder deze define draaide de display
op VSPI, waardoor alle signalen door de GPIO-matrix werden gerouteerd in plaats
van rechtstreeks. Dat werkt, maar kost latency en beperkt de haalbare klok.

De expliciete pin-defines in User_Setup.h overrulen sowieso de fallback-blokken
in `Processors/TFT_eSPI_ESP32.h:289-320` — dat zijn `#ifndef`-guards.

### Meetresultaten

Beide configuraties gebouwd, geflasht en gemeten op het bord met dezelfde
benchmark (`main/spi_benchmark.h`, N herhalingen per formaat,
`esp_timer_get_time()` eromheen).

| sprite | 27 MHz / VSPI | 40 MHz / HSPI | winst |
|---|---|---|---|
| 240x30 (banner) | 4,651 ms | **3,200 ms** | 1,45x |
| 240x160 (half scherm) | 24,733 ms | **17,009 ms** | 1,45x |
| doorvoer | 3,10 MB/s (24,8 MHz eff.) | **4,52 MB/s (36,1 MHz eff.)** | +46% |

**45% sneller, consistent over beide formaten.** Bij een half scherm scheelt dat
~8 ms per frame: het verschil tussen 40 en 59 fps.

De effectieve klok ligt ~10% onder de nominale door `setWindow`-overhead per push
(CASET/PASET/RAMWR dragen geen pixels). Die overhead is in beide configuraties
gelijk, dus de verhouding klopt.

### Status
- **40 MHz is stabiel.** Meermaals geflasht en geboot, geen crashes, schone
  boot-log, geen ruis.
- **Touch werkt na de poortwissel.** Kalibratie wordt geladen en gevalideerd
  (`Rotation MATCHES - calibration data is valid`), touch-events komen door.
- Bord draait nu op 40 MHz / HSPI, geverifieerd via serial na de laatste flash.
- Benchmark staat uitgecommentarieerd in `main/main.cpp` (include + aanroep),
  `main/spi_benchmark.h` blijft staan als referentiepunt.

### Correctie op een eerdere bewering
Tijdens deze sessie beweerde ik tussentijds dat een fullscreen 16-bpp sprite
"mogelijk wel past", op basis van de 167 KiB DRAM-regio uit `heap_init`. Dat was
onjuist. Alloceerbaar is het **grootste aaneengesloten blok** (110.592 bytes),
niet de vrije heap (204.144). De oorspronkelijke conclusie "past niet" was juist.

### Nog openstaand
- **80 MHz** te proberen. Buiten ILI9341-spec (datasheet noemt ~10 MHz voor
  schrijven), maar op een CYD met korte sporen vaak haalbaar. Falen is zichtbaar
  en onschuldig: ruis, verkeerde kleuren of bevroren beeld. Terugvallen kan door
  in User_Setup.h weer de 27 MHz-regel te activeren.
- **DMA staat uit** (`DMA_Enabled = false`, geen `initDMA()`-aanroep in het
  project). Met `initDMA()` + `pushImageDMA()` kan de CPU de volgende frame
  opbouwen tijdens de transfer van de vorige — daar is `createSprite(w, h, 2)`
  voor bedoeld. Grootste resterende winst na de kloksnelheid; de benchmark ligt
  klaar om het effect te meten.

