# Pong super, FreeRTOS-variant (pong_super_frt)
Dezelfde app als apps/pong_super, maar de eigen klassen gebruiken vanilla
FreeRTOS in plaats van CleanRTOS. Header prefix: psupf_, namespace:
pong_super_frt, app: Pong_super_frt.ino.

Omzetting (volgens de regels van ModelAnalysis, StdCleanRtosNaarStdFreeRtos):
- Task          -> eigen taak met xTaskCreatePinnedToCore (staticMain -> main())
- Flag          -> eventbit in de eventGroup van de klasse
- wait(flag)    -> xEventGroupWaitBits(..., pdTRUE, ...)
- waitAny/hasFired -> xEventGroupWaitBits(..., pdFALSE, ...) en per bit wissen
- Pool<T>       -> queue van lengte 1 (xQueueOverwrite / xQueuePeek)
- Timer         -> FreeRTOS software timer; de callback zet een eventbit
- ADC_buttons   -> frt::ADC_buttons uit libs/CYD_tools_frt
De control-objecten die in MainControl samengevoegd zijn (StartPageControl,
GameControl, ...) hebben elk een eigen eventGroup. De bibliotheken (CleanGUI,
AsyncDisplay) zijn wel CleanRTOS-gebaseerd; daarvoor staat MainInits nog in
Pong_super_frt_ino.h. Het model: models/pong_super_frt.drawio (de STD's zijn
met de tool omgezet naar FreeRTOS).

De tools/ van pong_super (ClearHallOfFame, ClearCalibrations) zijn niet
meegekopieerd: ze werken op dezelfde opslag en blijven daar.

---- Oorspronkelijke beschrijving (pong_super) ----

# Pong CleanGUI DG ADV (Decentralised Graphics, advanced)
Advanced variant of pong.


In this case, every "page" of the application get's its own DisplayControl,
and its own pageroot.

Advantage of this approach:
- Smaller, more clear DisplayControl files, compared to when all is poured 
  into one file.

Disadvantage of this approach:
- Multiple DisplayControl files needed. 


Furthermore, in this case, rendering via a backbuffer has been applied.

Advantage:
- Better framerate when lot's of widgets need to be shown each frame.

Disadvantage:
- The backbuffer takes memory. On ESP32, there is not enough memory to 
  facilitate an RGB56 framebuffer. For that reason, a one-byte-per-pixel
  paletted framebuffer is used. 

Paddle control via hardware buttons:
Besides the two touchscreen buttons, the paddle can also be moved with
buttons S1 (left) and S2 (right) of the resistor ladder on GPIO35
(crt::ADC_buttons, libs/CYD_tools_crt). PaddleControl listens to both;
they set the same flags. The ADC_buttons task is created in
Pong_super_frt_ino.h. Which ladder buttons are used is set by
AdcButtonLeft/AdcButtonRight in psupf_PaddleControl.h.

Button info page (first page after boot, shown only once):
Before the first start page, MainControl waits 200 ms (so that ADC_buttons has
done its boot check) and then shows ButtonInfoDisplayControl via
ButtonInfoControl. Depending on adcButtons.isConnected() it tells that the
external 6 button keyboard was detected (and that S1/S2 move the paddle), or
advises to connect it and reboot. "Ga verder" leads to the start page.
