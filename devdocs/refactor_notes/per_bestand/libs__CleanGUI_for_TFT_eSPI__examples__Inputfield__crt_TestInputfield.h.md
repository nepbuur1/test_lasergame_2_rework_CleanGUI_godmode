# Migratiebrief: `libs/CleanGUI_for_TFT_eSPI/examples/Inputfield/crt_TestInputfield.h`

4 call sites.

## Per call site

### regel 169 — `touchscreenKeyboard.disable(true);`  *(risico: mechanisch)*

**Bedoeling:** Touch uit tijdens de schuifanimatie; het commentaar op 152-154 zegt letterlijk dat moveTo _bVisible en _bEnabled met rust laat en dat het uitzetten van de touch daarom hier apart moet ('tikken op verschuivende toetsen slaat nergens op').

**Wordt:** ongewijzigd

### regel 205 — `touchscreenKeyboard.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Touch weer aan, maar alleen als het toetsenbord omhoog is geschoven ('alleen als hij in beeld staat'). Bij het wegschuiven blijft het bewust uitgeschakeld.

**Wordt:** ongewijzigd

### regel 252 — `page1.show(true);`  *(risico: stil-gedrag)*

**Bedoeling:** De eerste, volledige tekening van de pagina, met het toetsenbord net onder het scherm geparkeerd (regel 244-249).

**Wordt:** page1.redraw(true);  — show(true) zou hier tsOutsideInputfieldButton tekenbaar maken, die in crt_InputField.h:70 met /*drawable*/false wordt geconstrueerd.

### regel 308 — `inputfield.enable(true);`  *(risico: mechanisch)*

**Bedoeling:** Na het omhoogschuiven van het toetsenbord het invoerveld actief maken. Expliciet, dus onafhankelijk van welk neveneffect dan ook (de tegenhanger op regel 312 staat bewust uitgecommentarieerd).

**Wordt:** ongewijzigd

