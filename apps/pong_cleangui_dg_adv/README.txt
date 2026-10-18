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