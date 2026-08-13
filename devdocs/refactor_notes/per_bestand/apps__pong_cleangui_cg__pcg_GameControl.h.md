# Migratiebrief: `apps/pong_cleangui_cg/pcg_GameControl.h`

2 call sites.

## Per call site

### regel 49 — `paddleControl.enable();`  *(risico: mechanisch)*

**Bedoeling:** "De peddel eerst: die moet er staan voordat de bal eraan komt." Dit is PaddleControl::enable() (pcg_PaddleControl.h:113), die alleen flagEnable zet; PaddleControl vraagt daarna zelf displayControl.showPaddle() aan.

**Wordt:** Niet aanpassen. Geen widget-aanroep — alleen naamsgelijkenis. Een mechanische zoek-vervang op `enable(` mag hier niet toeslaan.

### regel 58 — `paddleControl.disable();`  *(risico: mechanisch)*

**Bedoeling:** "De peddelknoppen horen bij het spel, niet bij de game-over-pagina." Zet flagDisable; PaddleControl roept vervolgens displayControl.hidePaddle() aan, wat op DisplayControl-regel 576-581 uitkomt.

**Wordt:** Niet aanpassen. Geen widget-aanroep.

