# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/AsyncDisplay/crt_TestAsyncDisplay.h`

2 call sites.

## Per call site

### regel 143 — `page1.show(true);`  *(risico: mechanisch)*

**Bedoeling:** Elke lus-iteratie de hele boom opnieuw tekenen (achtergrond, paneel, negen Text-labels) — het uitgecommentarieerde ESP_LOGI op 142 noemt het 'clearing screen'. Puur tekenwerk; dit voorbeeld heeft geen enkele touch-listener, dus de enable-toestand doet er niet toe.

**Wordt:** page1.show(true); laten staan. redraw(true) is semantisch wat hier bedoeld wordt, maar mag alleen als regel 148 ook verandert — zie de opmerking daar: na hide(true) is het paneel non-drawable en zou een kale redraw het paneel voorgoed onzichtbaar maken.

### regel 148 — `panel.hide(true);`  *(risico: mechanisch)*

**Bedoeling:** Het paneel op zijn oude plek uitwissen vlak voordat setLocPos() op 149 het een pixel opschuift — de klassieke wis-verplaats-teken-animatie. Bedoeld is alleen 'veeg weg wat er nu staat'.

**Wordt:** panel.hide(true); kan blijven (makeNonDrawable + repaintRegion herstelt nu de achtergrond via de root in plaats van te vullen met _colBg; achtergrond is hier zwart, dus visueel gelijk). Netter is dit paar te vervangen door panel.moveTo(locPos, CoordType::Pixels), dat precies hiervoor bestaat en niet flikkert. Het meegedisabelde paneel en zijn labels maken hier niets uit.

