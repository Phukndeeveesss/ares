#ifdef PPU_CPP

void PPU::cgb_render() {
  for(auto& pixel : pixels) {
    pixel.color = 0x7fff;
    pixel.palette = 0;
    pixel.origin = Pixel::Origin::None;
  }

  if(status.display_enable) {
    cgb_render_bg();
    if(status.window_display_enable) cgb_render_window();
    if(status.ob_enable) cgb_render_ob();
  }

  uint32* output = screen + status.ly * 160;
  for(unsigned n = 0; n < 160; n++) output[n] = video.palette[pixels[n].color];
  interface->lcdScanline();
}

//Attributes:
//0x80: 0 = OAM priority, 1 = BG priority
//0x40: vertical flip
//0x20: horizontal flip
//0x08: VRAM bank#
//0x07: palette#
void PPU::cgb_read_tile(bool select, unsigned x, unsigned y, unsigned& tile, unsigned& attr, unsigned& data) {
  unsigned tmaddr = 0x1800 + (select << 10);
  tmaddr += (((y >> 3) << 5) + (x >> 3)) & 0x03ff;

  tile = vram[0x0000 + tmaddr];
  attr = vram[0x2000 + tmaddr];

  unsigned tdaddr = attr & 0x08 ? 0x2000 : 0x0000;
  if(status.bg_tiledata_select == 0) {
    tdaddr += 0x1000 + ((int8)tile << 4);
  } else {
    tdaddr += 0x0000 + (tile << 4);
  }

  y &= 7;
  if(attr & 0x40) y ^= 7;
  tdaddr += y << 1;

  data  = vram[tdaddr++] << 0;
  data |= vram[tdaddr++] << 8;
  if(attr & 0x20) data = hflip(data);
}

void PPU::cgb_render_bg() {
  unsigned iy = (status.ly + status.scy) & 255;
  unsigned ix = status.scx, tx = ix & 7;

  unsigned tile, attr, data;
  cgb_read_tile(status.bg_tilemap_select, ix, iy, tile, attr, data);

  for(unsigned ox = 0; ox < 160; ox++) {
    unsigned index = ((data & (0x0080 >> tx)) ? 1 : 0)
                   | ((data & (0x8000 >> tx)) ? 2 : 0);
    unsigned palette = ((attr & 0x07) << 2) + index;
    unsigned color = 0;
    color |= bgpd[(palette << 1) + 0] << 0;
    color |= bgpd[(palette << 1) + 1] << 8;
    color &= 0x7fff;

    pixels[ox].color = color;
    pixels[ox].palette = index;
    pixels[ox].origin = (attr & 0x80 ? Pixel::Origin::BGP : Pixel::Origin::BG);

    ix = (ix + 1) & 255;
    tx = (tx + 1) & 7;
    if(tx == 0) cgb_read_tile(status.bg_tilemap_select, ix, iy, tile, attr, data);
  }
}

void PPU::cgb_render_window() {
  if(status.ly - status.wy >= 144u) return;
  if(status.wx >= 167u) return;
  unsigned iy = status.wyc++;
  unsigned ix = (7 - status.wx) & 255, tx = ix & 7;

  unsigned tile, attr, data;
  cgb_read_tile(status.window_tilemap_select, ix, iy, tile, attr, data);

  for(unsigned ox = 0; ox < 160; ox++) {
    unsigned index = ((data & (0x0080 >> tx)) ? 1 : 0)
                   | ((data & (0x8000 >> tx)) ? 2 : 0);
    unsigned palette = ((attr & 0x07) << 2) + index;
    unsigned color = 0;
    color |= bgpd[(palette << 1) + 0] << 0;
    color |= bgpd[(palette << 1) + 1] << 8;
    color &= 0x7fff;

    if(ox - (status.wx - 7) < 160u) {
      pixels[ox].color = color;
      pixels[ox].palette = index;
      pixels[ox].origin = (attr & 0x80 ? Pixel::Origin::BGP : Pixel::Origin::BG);
    }

    ix = (ix + 1) & 255;
    tx = (tx + 1) & 7;
    if(tx == 0) cgb_read_tile(status.window_tilemap_select, ix, iy, tile, attr, data);
  }
}

//Attributes:
//0x80: 0 = OBJ above BG, 1 = BG above OBJ
//0x40: vertical flip
//0x20: horizontal flip
//0x08: VRAM bank#
//0x07: palette#
void PPU::cgb_render_ob() {
  const unsigned Height = (status.ob_size == 0 ? 8 : 16);
  unsigned sprite[10], sprites = 0;

  //find first ten sprites on this scanline
  for(unsigned s = 0; s < 40; s++) {
    unsigned sy = oam[(s << 2) + 0] - 16;
    unsigned sx = oam[(s << 2) + 1] -  8;

    sy = status.ly - sy;
    if(sy >= Height) continue;

    sprite[sprites++] = s;
    if(sprites == 10) break;
  }

  //render backwards, so that first sprite has highest priority
  for(signed s = sprites - 1; s >= 0; s--) {
    unsigned n = sprite[s] << 2;
    unsigned sy = oam[n + 0] - 16;
    unsigned sx = oam[n + 1] -  8;
    unsigned tile = oam[n + 2] & ~status.ob_size;
    unsigned attr = oam[n + 3];

    sy = status.ly - sy;
    if(sy >= Height) continue;
    if(attr & 0x40) sy ^= (Height - 1);

    unsigned tdaddr = (attr & 0x08 ? 0x2000 : 0x0000) + (tile << 4) + (sy << 1), data = 0;
    data |= vram[tdaddr++] << 0;
    data |= vram[tdaddr++] << 8;
    if(attr & 0x20) data = hflip(data);

    for(unsigned tx = 0; tx < 8; tx++) {
      unsigned index = ((data & (0x0080 >> tx)) ? 1 : 0)
                     | ((data & (0x8000 >> tx)) ? 2 : 0);
      if(index == 0) continue;

      unsigned palette = ((attr & 0x07) << 2) + index;
      unsigned color = 0;
      color |= obpd[(palette << 1) + 0] << 0;
      color |= obpd[(palette << 1) + 1] << 8;
      color &= 0x7fff;

      unsigned ox = sx + tx;
      if(ox < 160) {
        //When LCDC.D0 (BG enable) is off, OB is always rendered above BG+Window
        if(status.bg_enable) {
          if(attr & 0x80) {
            if(pixels[ox].origin == Pixel::Origin::BG || pixels[ox].origin == Pixel::Origin::BGP) {
              if(pixels[ox].palette > 0) continue;
            }
          }
        }
        pixels[ox].color = color;
        pixels[ox].palette = index;
        pixels[ox].origin = Pixel::Origin::OB;
      }
    }
  }
}

#endif