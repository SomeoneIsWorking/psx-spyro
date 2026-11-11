# Ideas

Maybes, not yet asked for as work.

- **Multiplayer, different coloured Spyros** (user, 2026-11-11). Each player needs their own view (the chase camera
  cannot frame two dragons): local split screen and LAN "everyone free". A second view is the draw section replayed
  in `GuestRedraw` with player 2's camera written. Player 2 is the player update run again with his state swapped
  into the global player struct (the Mega Man X4 co-op method), recoloured by a hue shift of his vertex colours.
  Open: co-op or competitive rules (gems, dragons, enemy targets, a Sparx each), moby activation and terrain LOD
  keyed to one player's distance, four section runs per logic frame at fps60, Spyro 1 first. Prior-art check
  before a plan.
