# Pong simple, FreeRTOS-variant (pong_simple_frt)
Dezelfde app als apps/pong_simple, maar de eigen klassen gebruiken vanilla
FreeRTOS in plaats van CleanRTOS. Header prefix: psimf_, namespace:
pong_simple_frt, app: Pong_simple_frt.ino.

Omzetting zoals bij apps_frt/pong_super_frt (zie de README daar):
Task -> xTaskCreatePinnedToCore, Flag -> eventbit, Pool<T> -> queue van
lengte 1, Timer -> FreeRTOS software timer met een callback die een eventbit
zet, waitAny/hasFired -> xEventGroupWaitBits en per bit wissen. De
control-objecten in MainControl (StartPageControl, GameControl,
GameOverControl) hebben elk een eigen eventGroup. De bibliotheken (CleanGUI,
AsyncDisplay) zijn wel CleanRTOS-gebaseerd; daarvoor staat MainInits nog in
Pong_simple_frt_ino.h. Het model: models/drawio/pong_simple_frt.drawio (de
STD's zijn met ModelAnalysis StdCleanRtosNaarStdFreeRtos omgezet naar
FreeRTOS).

---- Oorspronkelijke beschrijving (pong_simple) ----

# Pong CleanGUI CG (Centralised Graphics)
One object with a separate task is used to manage all rendering.

Advantage of this approach:
- Easy to switch to other graphics engine (like direct TFT_esp lib calls).
  (with a centralised DisplayControl control you only need to update the
   DisplayControl object, when porting the code to another engine)

Disadvantage of this approach:
- All display updates have to be transferred from the requesting
  tasks to the DisplayControl task. That gives call-overhead and 
  memory-overhead (for all the pools and queues that buffer the 
  transferred display parameters)
