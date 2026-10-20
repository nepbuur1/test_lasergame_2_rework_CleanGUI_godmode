2026, door Marius Versteegen

## Over deze workspace
Deze workspace bestaat uit een hierarchie van folders.
CleanRTOS based apps (voor mij normale apps), staan in de apps folder.
(NB: CleanRTOS is eigenlijk niets anders dan een verzameling handige c++ klassen
die FreeRTOS wrapt)

FreeRTOS based (voorbeeld-) apps staan in de apps_frt folder.

Het zal je opvallen dat de voorbeelden mijn CleanGUI graphics library gebruiken.
Onder de motorkap is die CLeanRTOS-, en dus ook FreeRTOS-based, en wrapt het Bodmers TFT_eSPI en TFT_eWidget libraries, waardoor die makkelijker te gebruiken worden.

## Template STD en Code
In apps/StateMachineTemplatePro vind je template STD en code van een klasse genaamd MachineControl. Je ziet daar hoe je op allerlei mogelijke manieren op al dan niet combinaties van eventbits, queues en timers kunt wachten.

## Over de voorbeelden
Kijk voor inspiratie voor je game project naar de voorbeelden in de apps_frt folder.
Daar vind je de apps:
- pong_cg_frt
  Een maximaal eenvoudige pong touchscreen game.
  - cg staat voor centralised graphics: 1 DisplayControl voor alle graphics.

- pong_dg_adv_frt
  Een (belachelijk?) ver geoptimaliseerde (en enigszins uitgebreide) pong game.
  - dg staat voor distributed graphics (een aparte Display Controller per "pagina" van de app)
  - adv staat voor advanced. De volgende features zijn inbegrepen:
    - Overal waar communicatie met ballen over en weer is, zijn taken samengevoegd,
    zodat er geen synchronisatie-overhead nodig is om die communicatie te plegen.
    Dus middels update/handler based, rtos-loze STDs, gebruikmakend van het FSM (Finite State Machine) Paradigm. (In de curus en in design-like a robot is deze verdoorgevoerde vorm van taaksamenvoeging niet besproken. Binnen deze game zijn daartoe dus van origine STDs voor freertos, rtos-loos gemaakt - werkend met bijvoorbeeld booleans ipv eventbits. Kijk bijvoorbeeld naar de STD van BallControl en naar de bijbehorende code hoe zoiets eruit ziet)
    - Er is een screenbuffer (8bpp paletted) gebruikt, met ondersteuning voor partieel-transparante graphics en animatieseries. De plaatjes worden rechtstreeks van flash geheugen naar naar die screenbuffer geblit.
    - De bronplaatjes zijn te vinden in de images subfolder. Die had ik kunnen tekenen met photoshop. Maar ik heb ze laten genereren door een LLM met make_pong_images.py binnen diezelfde folder.
    Bronplaatjes (pngs en jpgs) zijn weer naar paletted images geconverteerd mbv de python tools in de tools folder. De resulterende paletted images staan in de images/_pal8 subfolder
    - Er is een hall-of-fame waar highscores aan toegevoegd kunnen worden,
      met de naam van de speler erbij.
    - Er zijn kratten, vuurkorven en fakkels geimplementeerd.
    - Er kunnen maximaal 100 ballen gelijktijdig door het beeld vliegen.
    - Al die objecten zijn geprealloceerd, en gemanaged via een "Depot" object.
    - De game kan volledig met touchscreenbuttons gespeeld worden, maar detecteert het ook als een ADC_Buttons schakeling (met max 6 buttons, zie libs/CYD_tools_frt) aangesloten is. Daarmee kan de paddle dan (ook) bestuurd worden, ipv door gebruik van de trage/framerate verlagende touchscreen buttons.

Tip: Begin voor je eigen game met een zo eenvoudig mogelijke, speelbare basis. Kijk vervolgens stap voor stap wat je wilt toevoegen of optimaliseren. Zorg dat je je eigen code volledig begrijpt.

## eps-idf versie 5.4
De code in deze workspace is bedoeld voor versie 5.4 van de esp idf.

## Bouwen van Apps
Zelf vind ik het het fijnst om vanaf command line te bouwen. 
Na het starten van de esp idf, in deze folder:
```bash
idf.py build flash monitor
```
.. of evt met -p COM6 oid, als je meerdere devices hebt aangesloten, en er eentje wilt selecteren.

NB: om een app te bouwen, include die dan (als enigste app) in main/main.cpp, en pas main/CMakeList.txt aan (eenmalig): zet daarin de paden voor je compiler (de header file locaties) in COMPONENT_ADD_INCLUDEDIRS, en de paden voor je linker (de c en cpp file locaties) in COMPONENT_SRCS.

## Geschiedenis van dit project
Vroeger werd het gebruikt door studenten voor het maken van een lasergame. De bijbehorende repo is/heet test_lasergame_2_rework_CleanGUI, en geschikt voor esp-idf versie 5.3.

De naam godmode reflecteerde mijn intentie om snel aan de repo te developen met behulp van LLM's "in godmode" (dus met full control). Destijds was ik daar nogal voorzichtig mee, en heb daar een aparte github account voor gebruikt, vandaar de rare naam van het account (nepbuur1).

## Verdere bijzonderheden, LLM gebruik
Tegenwoordig laat ik LLM's veel van het typewerk doen. Vervolgens pas ik het aan, tot het helemaal voldoet aan wat voor mij logisch is. Ik heb bewust wat LLM gegenereerde comments en logboeken laten staan. 

Tip: probeer tijdens S3 zoveel mogelijk zelf te typen, en gebruik LLMs evt voor debugging.
Daardoor krijg je de stof beter in de vingers. Pas als je zo eenmaal de nodige ervaring hebt opgedaan, is het te overwegen de door jou gespecificeerde klassen door LLM's kunnen laten uittypen.