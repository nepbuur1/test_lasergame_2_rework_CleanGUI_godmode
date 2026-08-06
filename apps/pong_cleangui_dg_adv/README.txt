# Pong CleanGUI DG ADV (Decentralised Graphics, advanced)
Variant of apps/pong_cleangui_dg. Header prefix: pdga_, namespace:
pong_cleangui_dg_adv.

In this case, every "page" of the application get's its own DisplayControl,
and its own pageroot.

Advantage of this approach:
- Smaller, more clear DisplayControl files, then when all is poured 
  into one file.

Disadvantage of this approach:
- Multiple DisplayControl files needed. 