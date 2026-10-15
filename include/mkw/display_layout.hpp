#pragma once
namespace mkw::display {
// The top screen's canvas: 240 units high on every target. The 3DS top LCD is
// 5:3 (400 wide). The Vita's 960x544 is 16:9: the canvas widens to 424 at the
// same height, as the Wii's widescreen mode widens its 608x456 layouts.
#if defined(__vita__) || defined(__PPU__)
inline constexpr float Width=424.f;
#else
inline constexpr float Width=400.f;
#endif
inline constexpr float Height=240.f,Aspect=Width/Height,CenterX=Width/2;
// Render-target size of the canvas (portrait: TargetHeight is the canvas width).
inline constexpr unsigned TargetHeight=unsigned(Width);
// Added on the right of the 3DS's 400 units: right-anchored HUD moves by it.
inline constexpr float WideExtra=Width-400.f;
// Wii layout units (608x456 4:3, 832x456 16:9) to canvas units.
inline constexpr float MenuScaleX=Width/608.f,MenuScaleY=Height/456.f,WideScaleX=Width/832.f;
static_assert(Width==400.f||Width==424.f);
}
