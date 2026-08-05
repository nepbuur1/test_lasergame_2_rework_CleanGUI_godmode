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
