# Pong CleanGUI DG ADV (Decentralised Graphics, advanced)
Advanced variant of pong. Header prefix: pdga_, namespace:
pong_cleangui_dg_adv.


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
Pong_CleanGUI_dg_adv_ino.h. Which ladder buttons are used is set by
AdcButtonLeft/AdcButtonRight in pdga_PaddleControl.h.

Button info page (first page after boot, shown only once):
Before the first start page, MainControl waits 200 ms (so that ADC_buttons has
done its boot check) and then shows ButtonInfoDisplayControl via
ButtonInfoControl. Depending on adcButtons.isConnected() it tells that the
external 6 button keyboard was detected (and that S1/S2 move the paddle), or
advises to connect it and reboot. "Ga verder" leads to the start page.
