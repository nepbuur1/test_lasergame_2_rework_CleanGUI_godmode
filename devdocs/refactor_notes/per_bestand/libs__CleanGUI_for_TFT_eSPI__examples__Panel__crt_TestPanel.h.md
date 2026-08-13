# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/Panel/crt_TestPanel.h`

5 call sites.

## Per call site

### regel 149 — `panel.show(false);`  *(risico: mechanisch)*

**Bedoeling:** Elk paneel apart tekenen puur om het te kunnen timen; het commentaar op 146-147 zegt expliciet dat dit hetzelfde tekent als page1.show(true). Geen touch, geen enable in dit hele voorbeeld.

**Wordt:** panel.redraw(false);

### regel 151 — `panelImageCentered.show(false);`  *(risico: mechanisch)*

**Bedoeling:** Idem: het tweede paneel tekenen en de tijd ervan meten (emaCenteredUs).

**Wordt:** panelImageCentered.redraw(false);

### regel 153 — `panelImageClipped.show(false);`  *(risico: mechanisch)*

**Bedoeling:** Idem, geklipt plaatje, gemeten als emaClippedUs.

**Wordt:** panelImageClipped.redraw(false);

### regel 155 — `panelBackdrop.show(false);				// parent first,`  *(risico: mechanisch)*

**Bedoeling:** De ouder eerst tekenen (painter's algorithm), zodat de transparante ring erop kan.

**Wordt:** panelBackdrop.redraw(false);

### regel 157 — `panelImageTransparent.show(false);		// then its child on top.`  *(risico: mechanisch)*

**Bedoeling:** Het transparante kind erbovenop; bIncludeChildren=false is hier bewust, want de ouder is al apart getekend en getimed.

**Wordt:** panelImageTransparent.redraw(false);

