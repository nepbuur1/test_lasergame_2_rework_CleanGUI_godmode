# Pong

## Introductie

Doel is een eenvoudige voorbeeldgame, pong.

## Use Cases

| UC01_Main    |                                                                                                                                                                                                                                                    |
| ------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Beschrijving | Deze usecase coordineert wat het programma doet op het hoogste niveau: zijn we in het startscherm, spelen we het spel of zijn we in de game over afhandeling.                                                                                      |
| Scenario     | 1. Het startscherm wordt afgehandeld (zie UC02_GameStarting).<br/>2. De speler wordt in staat gesteld het spel te spelen (zie UC04_GamePlaying).<br/>3. De game over wordt afgehandeld (zie UC04_GameOverHandling).<br/>4. Terug naar stap 1.<br/> |

| UC02_GameStarting |                                                                                                                               |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Beschrijving      | Splash ervaring bij start van het spel geven.                                                                                 |
| Preconditie       | De main usecase (UC01_Main) heeft beslist dat dit nu moet gebeuren.                                                           |
| Scenario          | 1. De titel van het spel wordt getoond.<br/>2. Er is een "Start Game" knop zichtbaar.<br/>3. Wacht tot die knop is ingedrukt. |
| Postconditie      | Keer terug naar UC01_Main.                                                                                                    |

Nog leuke "Nice to Haves" om bovenstaand eventueel later toe te voegen:

- Opluisteren met animaties (van de Titel zelf, of tierelantijntjes erbij)

| UC03_GameOverHandling |                                                                                                                     |
| --------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Beschrijving          | Game over afhandeling.                                                                                              |
| Preconditie           | De main usecase (UC01_Main) heeft beslist dat dit nu moet gebeuren.                                                 |
| Scenario              | 1. Laat de tekst "Game Over zien".<br/>2. Wacht op een aanraking van het scherm of een timeout van 10 seconde.<br/> |
| Postconditie          | Keer terug naar UC01_Main.                                                                                          |

Nog leuke "Nice to Haves"" om bovenstaand eventueel later toe te voegen:

- Opluisteren met animaties (van de text Game Over zelf, of tierelantijntjes erbij).
- Een high score list laten zien.
- Bij voldoende score de speler zijn naam laten invoeren, om de high score list te updaten.

| UC04_GamePlaying |                                                                                                                                                                                                                                            |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Beschrijving     | Deze usecase bestuurt de organisatie van het spel                                                                                                                                                                                          |
| Preconditie      | De main usecase (UC01_Main) heeft beslist dat dit nu moet gebeuren.                                                                                                                                                                        |
| Scenario         | 1. Toon het spelbord<br/>2. Activeer de padlle<br/>3. Spawn een bal met random richting omhoog.<br/>4. Wacht tot er geen ballen meer in het spel zijn. (elke bal die de onderlijn waar de paddle langs beweegt, passeert, is uit het spel) |
| Invariant        | Als er gedurende 3 seconden geen bal de onderlijn is gepasseerd, wordt ergens in de bovenste helft van het spel een mijn gespawned (zie UC05_MineHandling)                                                                                 |
| Postconditie     | Keer terug naar UC01_Main.                                                                                                                                                                                                                 |

| UC05_MineHandling |                                                |
| ----------------- | ---------------------------------------------- |
| Beschrijving      | Deze usecase bestuurt het gedrag van een mijn. |
| Preconditie       | De mijn is gespawned door UC04_GamePlaying     |
| Scenario          | 1. Telkens als een bal de mijn raakt, verlaagt |


