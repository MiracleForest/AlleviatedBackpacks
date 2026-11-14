# Backpack model parts: element boxes with the dimensions of the
# block models (block pixels, 0..16, front = low z / north, left = high x), grouped the way
# BackpackDynamicModel composes its ModelPart values: BASE, FRONT_POUCH | BATTERY,
# LEFT_POUCH | LEFT_TANK, RIGHT_POUCH | RIGHT_TANK. The separate 'straps' group is our own
# split: it holds the rear strap boxes of the backpack_base.json, kept apart so they can
# be hidden when worn. Only the box sizes/positions and which faces exist are
# fixed (facts about the shape); UV layout and textures are our own.
#
# Each box: name, from, to, mat, faces.
#   mat: cloth   - tinted with the main colour
#        border  - tinted with the accent colour
#        plain   - border texture, untinted (the strap padding)
#        clips   - per-tier clips texture, untinted
#        modules - tank / battery texture, untinted
#        glass   - tank glass, translucent
#   faces: subset of 'nsewud' (north south east west up down) that exist.

$script:BackpackParts = [ordered]@{
    base        = @(
        @{ n = 'bottom_trim';  f = 4.25, 8.25, 4.75; t = 11.75, 9.25, 5;     m = 'border';  x = 'nsewud' }
        @{ n = 'right_trim';   f = 3.25, 8.25, 4.75; t = 4.25, 13.25, 10.75; m = 'border';  x = 'nsewud' }
        @{ n = 'lip';          f = 4.25, 9.25, 4.75; t = 11.75, 13.25, 10.75; m = 'cloth';  x = 'nsewud' }
        @{ n = 'left_trim';    f = 11.75, 8.25, 4.75; t = 12.75, 13.25, 10.75; m = 'border'; x = 'nsewud' }
        @{ n = 'handle_left';  f = 9.5, 13.25, 7.75; t = 10.25, 14, 8.75;    m = 'clips';   x = 'nsew' }
        @{ n = 'handle_top';   f = 5.75, 14, 7.75;   t = 10.25, 14.5, 8.75;  m = 'clips';   x = 'nsewu' }
        @{ n = 'handle_right'; f = 5.75, 13.25, 7.75; t = 6.5, 14, 8.75;     m = 'clips';   x = 'nsew' }
        @{ n = 'strap_left';   f = 10.5, 9, 4.5;     t = 11.5, 13.5, 11.25;  m = 'clips';   x = 'nsewu' }
        @{ n = 'clip_left';    f = 10.5, 7, 4.5;     t = 11.5, 9, 5.25;      m = 'clips';   x = 'nsewud' }
        @{ n = 'strap_right';  f = 4.5, 9, 4.5;      t = 5.5, 13.5, 11.25;   m = 'clips';   x = 'nsewu' }
        @{ n = 'clip_right';   f = 4.5, 7, 4.5;      t = 5.5, 9, 5.25;       m = 'clips';   x = 'nsewud' }
        @{ n = 'body';         f = 3, 0, 5;          t = 13, 13, 11;         m = 'cloth';   x = 'nsewud' }
    )
    front_pouch = @(
        @{ n = 'top';        f = 4, 2, 3;      t = 12, 6, 5;  m = 'cloth';  x = 'newu' }
        @{ n = 'trim';       f = 4, 1, 3;      t = 12, 2, 5;  m = 'border'; x = 'new' }
        @{ n = 'bottom';     f = 4, 0, 3;      t = 12, 1, 5;  m = 'cloth';  x = 'newd' }
        @{ n = 'clip_left';  f = 10, 3, 2.75;  t = 11, 5, 3;  m = 'clips';  x = 'newud' }
        @{ n = 'clip_right'; f = 5, 3, 2.75;   t = 6, 5, 3;   m = 'clips';  x = 'newud' }
        @{ n = 'opening';    f = 4, 4, 3;      t = 12, 5, 5;  m = 'clips';  x = 'new' }
    )
    left_pouch  = @(
        @{ n = 'top';       f = 13, 2, 5.5;   t = 15, 6, 10.5;    m = 'cloth';  x = 'nesu' }
        @{ n = 'trim';      f = 13, 1, 5.5;   t = 15, 2, 10.5;    m = 'border'; x = 'nes' }
        @{ n = 'bottom';    f = 13, 0, 5.5;   t = 15, 1, 10.5;    m = 'cloth';  x = 'nesd' }
        @{ n = 'clip';      f = 15, 3, 7.5;   t = 15.25, 5, 8.5;  m = 'clips';  x = 'nesud' }
        @{ n = 'opening';   f = 13, 4, 5.5;   t = 15, 5, 10.5;    m = 'clips';  x = 'nes' }
        @{ n = 'pouch';     f = 13, 7, 5.5;   t = 14, 11, 10.5;   m = 'cloth';  x = 'nesud' }
        @{ n = 'clip2';     f = 14, 8, 7.5;   t = 14.25, 10, 8.5; m = 'clips';  x = 'nesud' }
        @{ n = 'opening2';  f = 12, 9, 5.5;   t = 14, 10, 10.5;   m = 'clips';  x = 'nes' }
    )
    right_pouch = @(
        @{ n = 'pouch';     f = 2, 7, 5.5;    t = 3, 11, 10.5;    m = 'cloth';  x = 'nswud' }
        @{ n = 'clip2';     f = 1.75, 8, 7.5; t = 2, 10, 8.5;     m = 'clips';  x = 'nswud' }
        @{ n = 'opening2';  f = 2, 9, 5.5;    t = 4, 10, 10.5;    m = 'clips';  x = 'nsw' }
        @{ n = 'top';       f = 1, 2, 5.5;    t = 3, 6, 10.5;     m = 'cloth';  x = 'nswu' }
        @{ n = 'trim';      f = 1, 1, 5.5;    t = 3, 2, 10.5;     m = 'border'; x = 'nsw' }
        @{ n = 'bottom';    f = 1, 0, 5.5;    t = 3, 1, 10.5;     m = 'cloth';  x = 'nswd' }
        @{ n = 'clip';      f = 0.75, 3, 7.5; t = 1, 5, 8.5;      m = 'clips';  x = 'nswud' }
        @{ n = 'opening';   f = 1, 4, 5.5;    t = 3, 5, 10.5;     m = 'clips';  x = 'nsw' }
    )
    left_tank   = @(
        @{ n = 'back';        f = 12.5, 1.5, 6;   t = 13.5, 7.5, 10;  m = 'modules'; x = 'nsewud' }
        @{ n = 'cap';         f = 12.5, 10.5, 6.5; t = 15.5, 11.5, 9.5; m = 'modules'; x = 'nsewud' }
        @{ n = 'cap_bottom';  f = 13, 9.5, 6;     t = 16, 10.5, 10;   m = 'modules'; x = 'nsewud' }
        @{ n = 'trim';        f = 13, 8.5, 6;     t = 16, 9.5, 10;    m = 'border';  x = 'nsewud' }
        @{ n = 'top';         f = 13, 7.5, 6;     t = 16, 8.5, 10;    m = 'modules'; x = 'nsewud' }
        @{ n = 'bottom';      f = 13, 0.5, 6;     t = 16, 1.5, 10;    m = 'modules'; x = 'nsewud' }
        @{ n = 'glass_front'; f = 15.5, 1.5, 6.5; t = 15.5, 7.5, 9.5; m = 'glass';   x = 'nsewud' }
        @{ n = 'glass_left';  f = 13.5, 1.5, 9.5; t = 15.5, 7.5, 9.5; m = 'glass';   x = 'nsewud' }
        @{ n = 'glass_right'; f = 13.5, 1.5, 6.5; t = 15.5, 7.5, 6.5; m = 'glass';   x = 'nsewud' }
    )
    right_tank  = @(
        @{ n = 'back';        f = 2.5, 1.5, 6;    t = 3.5, 7.5, 10;   m = 'modules'; x = 'nsewud' }
        @{ n = 'cap';         f = 0.5, 10.5, 6.5; t = 3.5, 11.5, 9.5; m = 'modules'; x = 'nsewud' }
        @{ n = 'cap_bottom';  f = 0, 9.5, 6;      t = 3, 10.5, 10;    m = 'modules'; x = 'nsewud' }
        @{ n = 'trim';        f = 0, 8.5, 6;      t = 3, 9.5, 10;     m = 'border';  x = 'nsewud' }
        @{ n = 'top';         f = 0, 7.5, 6;      t = 3, 8.5, 10;     m = 'modules'; x = 'nsewud' }
        @{ n = 'bottom';      f = 0, 0.5, 6;      t = 3, 1.5, 10;     m = 'modules'; x = 'nsewud' }
        @{ n = 'glass_front'; f = 0.5, 1.5, 6.5;  t = 0.5, 7.5, 9.5;  m = 'glass';   x = 'nsewud' }
        @{ n = 'glass_right'; f = 0.5, 1.5, 9.5;  t = 2.5, 7.5, 9.5;  m = 'glass';   x = 'nsewud' }
        @{ n = 'glass_left';  f = 0.5, 1.5, 6.5;  t = 2.5, 7.5, 6.5;  m = 'glass';   x = 'nsewud' }
    )
    battery     = @(
        @{ n = 'battery';      f = 4, 0, 2;          t = 12, 6, 5;          m = 'modules'; x = 'nsewud' }
        @{ n = 'connection1';  f = 9, 5.25, 3.5;     t = 10, 6.25, 4.5;     m = 'modules'; x = 'nsewud' }
        @{ n = 'connection2';  f = 7.75, 5.25, 3.5;  t = 8.75, 6.25, 4.5;   m = 'modules'; x = 'nsewud' }
        @{ n = 'connection3';  f = 9, 7, 4.75;       t = 10, 8, 5.75;       m = 'modules'; x = 'nsewud' }
        @{ n = 'connection4';  f = 7.75, 7, 4.75;    t = 8.75, 8, 5.75;     m = 'modules'; x = 'nsewud' }
        @{ n = 'black_wire1';  f = 9.2, 5.6, 3.7;    t = 9.8, 7.2, 4.3;     m = 'modules'; x = 'nsewud' }
        @{ n = 'black_wire2';  f = 9.2, 7.2, 3.7;    t = 9.8, 7.8, 5.3;     m = 'modules'; x = 'nsewud' }
        @{ n = 'red_wire1';    f = 7.95, 5.6, 3.7;   t = 8.55, 7.2, 4.3;    m = 'modules'; x = 'nsewud' }
        @{ n = 'red_wire2';    f = 7.95, 7.2, 3.7;   t = 8.55, 7.8, 5.3;    m = 'modules'; x = 'nsewud' }
        @{ n = 'clip_top';     f = 5.8, 4.05, 1.65;  t = 7.2, 5.45, 3.05;   m = 'clips';   x = 'nsewud' }
        @{ n = 'clip_bottom';  f = 5.8, 0.05, 1.65;  t = 7.2, 1.45, 3.05;   m = 'clips';   x = 'nsewud' }
        @{ n = 'strap_tr';     f = 3.75, 4.25, 1.75; t = 4.75, 5.25, 5.75;  m = 'border';  x = 'nsewud' }
        @{ n = 'strap_tm';     f = 4.5, 4.25, 1.75;  t = 11.5, 5.25, 2.75;  m = 'border';  x = 'nsewud' }
        @{ n = 'strap_tl';     f = 11.25, 4.25, 1.75; t = 12.25, 5.25, 5.75; m = 'border'; x = 'nsewud' }
        @{ n = 'strap_br';     f = 3.75, 0.25, 1.75; t = 4.75, 1.25, 5.75;  m = 'border';  x = 'nsewud' }
        @{ n = 'strap_bm';     f = 4.5, 0.25, 1.75;  t = 11.5, 1.25, 2.75;  m = 'border';  x = 'nsewud' }
        @{ n = 'strap_bl';     f = 11.25, 0.25, 1.75; t = 12.25, 1.25, 5.75; m = 'border'; x = 'nsewud' }
    )
    straps      = @(
        @{ n = 'inner_l';  f = 10.25, 0, 11; t = 10.75, 12.5, 12; m = 'cloth';  x = 'nsewud' }
        @{ n = 'trim_l';   f = 10.75, 0, 11; t = 11.75, 12.5, 12; m = 'border'; x = 'nsewud' }
        @{ n = 'outer_l';  f = 11.75, 0, 11; t = 12.25, 12.5, 12; m = 'cloth';  x = 'nsewud' }
        @{ n = 'inner_r';  f = 5.25, 0, 11;  t = 5.75, 12.5, 12;  m = 'cloth';  x = 'nsewud' }
        @{ n = 'trim_r';   f = 4.25, 0, 11;  t = 5.25, 12.5, 12;  m = 'border'; x = 'nsewud' }
        @{ n = 'outer_r';  f = 3.75, 0, 11;  t = 4.25, 12.5, 12;  m = 'cloth';  x = 'nsewud' }
        @{ n = 'padding';  f = 4, 1, 11;     t = 12, 12, 11.5;    m = 'plain';  x = 'nsewud' }
    )
}

# Dynamic boxes, positioned as the BackpackDynamicModel draws them:
#   charge - addCharge: a north-facing quad at z 1.95, y 2..3, filling x (10 - pixels)..10 for
#            pixels = 0..4 (we pre-build the 4 steps).
#   fluid  - addFluid: x xMin..xMin + 2.5 (xMin 12.85 left, 0.6 right), y 1.5..1.5 + 6 * ratio,
#            z 6.75..9.25 (we pre-build 10 fill steps).
$script:BatteryCharge = @{ f = 6, 2, 1.95; t = 10, 3, 1.95; steps = 4 }
$script:TankFluid = @{
    left  = @{ f = 12.85, 1.5, 6.75; t = 15.35, 7.5, 9.25; x = 'nse'; steps = 10 }
    right = @{ f = 0.6, 1.5, 6.75;   t = 3.1, 7.5, 9.25;   x = 'nsw'; steps = 10 }
}
# Displayed-item anchor: BackpackBlockEntityRenderer.renderItemDisplay translates the
# item by (0, 0.6, 0.25) block from the block centre (front = low z) and scales it by 0.5, so the
# 16 px item becomes an 8 px square centred at (8, 9.6, 4). Only its centre is used.
$script:DisplayAnchor = @{ f = 4, 5.6, 4; t = 12, 13.6, 4 }