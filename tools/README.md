# tools

Gereedschap voor het hele project. Wat hier staat is niet van één example of app:
`img2rgb565.py` bedient elke images-boom die je aanlegt, `drawio-export.sh` elk
model onder `apps/*/models/`.

Het meeste is Python en deelt de venv hieronder; `drawio-export.sh` is bash en
heeft er niets van nodig.

## Eenmalig opzetten

```
bash tools/setup_venv.sh
```

Dat maakt `tools/venv` aan en installeert `requirements.txt` erin (nu alleen
Pillow). De map `venv` staat in `.gitignore` — hij hoort niet in de repo, en
`requirements.txt` volstaat om hem elders opnieuw op te bouwen.

**Je hoeft de venv nooit te activeren.** Elk script begint met
`ensure_venv()` uit `venv_bootstrap.py`, dat kijkt of `sys.prefix` al naar
`tools/venv` wijst en zo niet, zichzelf opnieuw start met `tools/venv/bin/python`.
Een kale `python3 tools/…` is dus genoeg. Ontbreekt de venv, dan zegt het script
precies dat je `setup_venv.sh` moet draaien.

## img2rgb565.py — één plaatje omzetten

```
python3 tools/img2rgb565.py level1/characters/speler.png
```

Zet elk door Pillow leesbaar beeld (PNG/JPG/BMP/GIF) om naar een header met een
`const uint16_t[]` in RGB565. Die array belandt in `.rodata`, dus in flash: hij
kost geen DRAM en wordt nooit gekopieerd — de DisplayAdapter blit er rechtstreeks
uit.

Het plaatje moet ergens onder een map **`images`** staan. Die map is de wortel:
het pad eronder wordt platgeslagen tot de outputnaam, zodat twee plaatjes met
dezelfde bestandsnaam in verschillende submappen elkaar nooit overschrijven.

```
<images>/level1/characters/speler.png
    -> <images>/_rgb565/level1_characters_speler.h
```

Het pad naar het plaatje mag relatief of absoluut zijn en je mag het commando
vanuit elke map geven; de tool zoekt de `images`-wortel zelf op.

### Parameters in de bestandsnaam

Parameters komen achteraan de naam, elk beginnend met een `-`:

| parameter | betekenis |
|---|---|
| `-trcolor_RRGGBB` | deze kleur geldt als transparant |
| `-ssh_RxC` | het plaatje is een spritesheet van R rijen bij C kolommen |

`-trcolor` doet twee verschillende dingen, afhankelijk van de bron:

- **Bron mét alpha-kanaal:** elke pixel met alpha < 128 wordt weggeschreven ín
  de opgegeven kleur. De transparantie zit daarna dus in de kleur zelf.
- **Bron zonder alpha-kanaal:** de omzetting is precies dezelfde als zónder de
  parameter. De kleur is dan puur een aanwijzing die via `Image::colTransparent`
  wordt doorgegeven aan wat het plaatje blit.

`-ssh` wordt alleen vastgelegd in `Image::nofRows` / `Image::nofCols`. CleanGUI
doet er voorlopig nog niets mee.

```
level1/characters/speler-trcolor_FF00FF-ssh_10x5.png
    -> _rgb565/level1_characters_speler-trcolor_FF00FF-ssh_10x5.h
```

De parameters blijven in de naam van de header staan — dat houdt hem uniek en
zelfverklarend — maar verdwijnen uit de C++-namen erbinnen; daar worden ze
constructor-argumenten van de `Image`:

```cpp
arLevel1CharactersSpelerPixels[]   // in de gegenereerde header
imageLevel1CharactersSpeler        // in _rgb565/_allImages.h
```

Omdat `-` de parameters scheidt, kan een `-` niet in een beeldnaam zelf zitten.
De tool zegt dat ook als je het toch probeert.

### `_allImages.h`

Naast een header per plaatje schrijft de tool `_rgb565/_allImages.h`. Daarin
staat één `constexpr Image` per array — met de afmetingen, de transparante kleur
en de spritesheet-indeling ingevuld — plus `arAllImages[]`, `arAllImageNames[]`
en `nofAllImages`, zodat een viewer of test alle plaatjes kan aflopen zonder ze
stuk voor stuk te noemen. **Dit is het bestand dat je includeert**, niet de
losse pixel-headers.

Het bestand wordt volledig herbouwd uit de headers die naast hem staan. Elke
gegenereerde header draagt daarvoor een `// crt-img-meta:`-regel; er is dus geen
apart administratiebestand dat kan verouderen of zoekraken.

## allImg2rgb565.py — de hele boom bijwerken

```
python3 tools/allImg2rgb565.py [<images-map>]
```

Zonder argument geldt de huidige map. Converteert alles wat veranderd is sinds
de vorige keer, en herbouwt `_allImages.h`.

"Veranderd" betekent: het bestand is nieuwer dan de tijdstempel in
`allImg2rgb565_last_called.txt` (die in de images-wortel staat), **óf** de
bijbehorende header ontbreekt. Die tweede voorwaarde is belangrijker dan hij
lijkt: gooi je `_rgb565` weg, of doet iemand een verse checkout, dan is elke bron
"ouder dan de laatste run" en zou er zonder die voorwaarde niets gebeuren.

De weggeschreven tijdstempel is het moment waarop de run **begon**, niet waarop
hij klaar was. Een plaatje dat je bewerkt terwijl de tool draait, valt anders
precies in het gat tussen die twee en wordt nooit meer opgepikt. Nu wordt het
hooguit één keer te vaak omgezet, en nooit gemist. Gaat er iets mis, dan wordt de
stempel helemaal niet geschreven en probeert de volgende run het opnieuw.

Vlaggen:

- `--all` — negeer de tijdstempel en zet alles om.
- `--prune` — verwijder headers waarvan het bronplaatje weg is. Zonder deze vlag
  worden ze alleen gemeld; gegenereerde bestanden die in git staan wis je niet
  zomaar achter iemands rug.

## Voorbeeld

`libs/CleanGUI_for_TFT_eSPI/examples/ImageViewer` gebruikt dit alles: bronnen in
`images/`, gegenereerde headers in `images/_rgb565/`, en een viewer die
`arAllImages[]` afloopt met twee touchknoppen.

## drawio-export.sh — een modelpagina bekijken

```
tools/drawio-export.sh <bestand.drawio> --list
tools/drawio-export.sh <bestand.drawio> <paginanaam> [uitvoer.png] [schaal]
```

Rendert één pagina uit een `.drawio` naar PNG/SVG/PDF, door vanuit WSL de draw.io
desktop-app op de Windows-kant aan te roepen. Het formaat volgt de extensie van de
uitvoernaam; de schaal (default 2) geldt alleen voor bitmaps.

```
tools/drawio-export.sh apps/pong_cleangui_dg_adv/models/pong_dg_adv.drawio \
    "STD BallControl" /tmp/ball.png
```

Waarom dit bestaat: de modellen zijn platte mxGraph-XML, dus je kunt ze met
tekstgereedschap doorzoeken en bewerken — maar niet bekíjken, en zonder kijken is
een diagram niet te controleren. Dit sluit die lus.

**Op naam, niet op nummer.** `--page-index` van draw.io is 1-gebaseerd ondanks de
naam (0 levert stilzwijgend niets op), en belangrijker: die nummers schuiven zodra
er een pagina bijkomt. Een paginanaam blijft staan. Vandaar dat het script de
lijst zelf opzoekt en jij nooit een index hoeft te noemen.

**Het bestand moet dicht zijn in draw.io** als er ook geschreven wordt. Alleen
exporteren mag met een open bestand, maar draw.io houdt de inhoud in het geheugen
en schrijft die bij de eerstvolgende save volledig terug — wijzigingen die
ondertussen van buitenaf in het bestand kwamen, zijn dan weg.

Vindt het script draw.io niet, zet dan het pad in de omgeving:

```
DRAWIO="/mnt/c/Program Files/draw.io/draw.io.exe" tools/drawio-export.sh ...
```

De backupbestanden die draw.io naast een open model neerzet (`.$naam.drawio.bkp`)
horen niet in de repo; `*.bkp` staat daarom in `.gitignore`.
