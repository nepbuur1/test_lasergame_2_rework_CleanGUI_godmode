
// This is the command sequence that rotates the ILI9341 driver coordinate frame

  rotation = m % 8; // Limit the range of values to 0-7

  // CYD-uitbreiding: bij een paneel dat van zichzelf liggend staat de
  // MADCTL-waarde van een kwartslag verder (zie setPanelRotationOffset in
  // TFT_eSPI.h). Breedte en hoogte worden onderaan alsnog op de gevraagde
  // rotatie gezet. _panelMadctlXor draait bits van MADCTL om; bij dat paneel de
  // kleurvolgorde, want het heeft rood en blauw andersom (setPanelColorOrderSwapped).
  uint8_t madRotation = rotation;
  if (rotation < 4) madRotation = (rotation + _panelRotationOffset) % 4;

  writecommand(TFT_MADCTL);
  switch (madRotation) {
    case 0:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_width;
      _height = _init_height;
      break;
    case 1:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_height;
      _height = _init_width;
      break;
    case 2:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MY | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_width;
      _height = _init_height;
      break;
    case 3:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_height;
      _height = _init_width;
      break;
  // These next rotations are for bottom up BMP drawing
    case 4:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_width;
      _height = _init_height;
      break;
    case 5:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MY | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_height;
      _height = _init_width;
      break;
    case 6:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_width;
      _height = _init_height;
      break;
    case 7:
#ifdef M5STACK
      writedata(_panelMadctlXor ^ (TFT_MAD_MX | TFT_MAD_COLOR_ORDER));
#else
      writedata(_panelMadctlXor ^ (TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER));
#endif
      _width  = _init_height;
      _height = _init_width;
      break;

  }

  // De afmetingen horen bij de gevraagde rotatie, niet bij madRotation. Zonder
  // offset is dat hetzelfde als wat de cases hierboven al zetten.
  if (rotation & 1) { _width = _init_height; _height = _init_width; }
  else              { _width = _init_width;  _height = _init_height; }
