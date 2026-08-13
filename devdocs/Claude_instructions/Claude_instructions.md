# Claude Instructions
## Introduction
This file contains tasks for Claude. Claude should only perform such a task after explicit request.

## Overzicht
| # | taak | status | beschrijving |
|---|---|---|---|
| 1 | Image-assets in flash zonder CMakeLists-wijzigingen | gedaan | hieronder |
| 2 | CleanGUI: drie orthogonale assen (show / enable / draw) | uitgevoerd, wacht op handtest | [CleanGUI_show_enable_refactor.md](CleanGUI_show_enable_refactor.md) |

Task 2 is uitgevoerd op de branch `show_drawable_enable_refactoring`, alle tien
de stappen, één commit per stap. Wat er nog met een vinger op het scherm
gecontroleerd moet worden staat in `MIGRATION_STATUS.md` in de repo-root; het
verslag staat in het logboek onder 2026-08-13. Na die handtest kan de branch
naar `main`.

## Task 1
Eerder had je een mooi plan gemaakt:

Image-assets in flash zonder CMakeLists-wijzigingen

 Context

 Via Panel in CleanGUI kunnen nu bitmap images direct uit flash geheugen 
 getoond worden (zie CleanGUI_for_TFT_eSPI/examples/Panel).

 Ik wil nu graag een extra example toegevoegd in de examples folder, genaamd "ImageViewer".
 Die heeft "dan een subfolder genaamd "images".

 In die folder en/of haar subfolders staan dan images van elk door Pillow leesbaar beeld (PNG/JPG/BMP/GIF).
 Die kunnen met onderstaande, te ontwikkelen converter-tool omgezet worden naar RGB565 header files, zoals gebruikt in de Panel Demo. 

 Voorbeeld van hoe het python script werkt:
 In de images folder wordt aangeroepen:
 python3 tools/img2rgb565.py level1/characters/speler.png

 (NB: level1 is dus een subfolder van images)
 -> dit generereert level1_characters_speler.h met de RGB565 in de _rgb565 (output-) subfolder of images.

 Bij een zekere postfix van de filenaam wordt er transparency gebruikt:
 python3 tools/img2rgb565.py level1/characters/speler-trcolor_FF00FF.png
 -> dit genererert speler-trcolor_FF00FF.h in de output folder.
 In dit geval: als de bron-image een plaatje zonder alpha-layer was,
 wordt de omzetting gedaan alsof de postfix er niet zou zijn.
 De postfix in de naam is in dat geval alleen een hint aan de engine die de image gaat blitten.
 Als de bron-image een plaatje met alpha-layer is, wordt bij alpha kleiner 
 dan 128 (helft van maximaal) de tranparent-color (in dit voorbeeld 0xFF00FF) ingevuld
 voor de betreffende pixel.
 naast -trcolor als postfix is er nog een postfix mogelijk: -ssh, die specificeert een 
 spritesheet. bijvoorbeeld: 
python3 tools/img2rgb565.py level1/characters/speler-trcolor_FF00FF-ssh_10x5.png geeft 
aan dat het plaatje een spritesheet is, met 10 rijen en 5 kolommen.
Daar hoeft nu nog niets mee gedaan te worden. Dat doen we in een volgende ronde.

 Verder moet mogelijk zijn:
 python3 tools/allImg2rgb565.py
 die controleert welke files zijn veranderd in de image tree sinds de laatste call van allImg2rgb565.py,
 en converteert die (opnieuw, oude output overschrijvend).
 Daarvoor wordt gebruik gemaakt van het modified attribuut van de file in het os.
 Verder wordt ervoor gebruik gemaakt van een allImg2rgb565_last_called.txt file, waarin de 
 datetime na afronding van de laatste call van allImg2rgb565.py wordt opgeslagen.

 Verder: ik wil graag dat de tools gebruik maken van een eigen virtual environment,
 middels een venv folder die subfolder is van de tools folder. 
 Verder wil ik in de tools folder graag een requirements.txt, waarmee de venv folder
 via een pip install geinitaliseerd kan worden. ik wil ook graag dat .claudeignore en .gitignore
 worden aangepast, zodat niet de contents van de venv folder en de python build in git terecht komt.
 