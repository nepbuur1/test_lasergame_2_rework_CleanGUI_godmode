# Pong CleanGUI DG (Decentralised Graphics)
When we assume to be using CleanGUI, which already is a centralised graphics engine,
there is no need to further centralise the game objects of our game-engine.
Each game object can manage its drawables itself.

Advantage of this approach:
- No (additional) DisplayControl object needed, so simpler design.

Disadvantage of this approach:
- Not easy to switch to other graphics engine (like direct TFT_esp lib calls).
  (with a centralised DisplayControl control you only need to update the
   DisplayControl object, when porting the code to another engine)