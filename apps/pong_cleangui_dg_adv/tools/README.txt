Gereedschap bij pong_cleangui_dg_adv
====================================

Kleine losse programma's die iets aan de opslag op het bord doen. Elk krijgt
zijn eigen submap met een .ino en de bijbehorende _ino.h, net als de voorbeelden
in libs/. Wie er een bij zet, zet de map ook in COMPONENT_ADD_INCLUDEDIRS van
main/CMakeLists.txt.

Draaien gaat net als bij elk ander programma in deze repo: in main/main.cpp mag
precies een #include van een .ino actief zijn. Zet die van het gereedschap aan,
bouw, flash, kijk in de monitor, en zet main.cpp daarna terug op de app.

    idf.py build
    idf.py -p /dev/ttyUSB0 flash monitor

Allebei drukken ze hun eigen oordeel af - GEWIST of MISLUKT - en allebei laten
ze LITTLEFS.format() met rust: dat zou het andere bestand meenemen.


ClearHallOfFame/
----------------
Wist de top-5 in /hallOfFame. Schrijft er een lege lijst van de huidige
bestandsversie voor terug en leest die daarna opnieuw in om te laten zien dat
het gelukt is. Geen scherm of aanraking nodig, en ongevaarlijk voor de rest.

Er zit een schakelaar in, CLEAR_HALL_OF_FAME_SELFTEST, die standaard uit staat.
Op 1 schrijft het programma eerst een verzonnen lijst weg en wist die daarna,
zodat "GEWIST" ook echt betekent dat er iets te wissen was. Zonder die stap zegt
een geslaagde run op een al lege lijst niets.

Weggooien in plaats van leegschrijven zou hier ook gekund hebben; een leeg
bestand van de juiste versie houdt de leesroutine van de app meteen onder de
controle.


ClearCalibrations/
------------------
Wist de touchkalibratie in /TouchCalData1: meldt eerst welke van de vier
schermrotaties gekalibreerd waren en gooit het bestand daarna weg.

LET OP - dit kost een vinger om terug te krijgen. Daarna vraagt elke app bij het
opstarten weer om een kalibratie voor zijn eigen rotatie. Draai het voorbeeld
TouchCalibrateAll, dan staan alle vier de rotaties weer klaar.

Hier wordt het bestand wel weggegooid en niet leeg teruggeschreven. Dat scheelt
een tweede plek die de v2-indeling moet kennen; lezen doet het programma wel,
want dat is wat de melding vooraf de moeite waard maakt. Een ontbrekend bestand
is bovendien de weg die de DisplayAdapter zelf al netjes afhandelt.

De kalibratie is niet van deze app maar van het bord - alle apps en voorbeelden
delen hem. Dit gereedschap staat hier omdat de gereedschapskist hier staat.
