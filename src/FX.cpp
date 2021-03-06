#include <FX.h>

// ----------------------------------------------------
  // better for startup: no blinking, strips is initialized before to 0 brightness
void FX::init(int nLeds)
{
  mNLEDS = nLeds;
  mLeds = (CRGB* )malloc(nLeds * sizeof(CRGB));
  assert (mLeds!=nullptr);
  ClearLeds(mLeds, nLeds);

  AddVarCode("alpha", setAlpha(args[0]),  getAlpha(), 255, 0, 255) // default visible
  initFX();
}

void FX::setAlpha(const byte alpha) 
{ 
  mAlpha = ((alpha + 1) * alpha) >> 8; // fast gamma
  mLinearAlpha = alpha; 
}

void FX::setAlphaMul(const byte a1, const byte a2)
{
  setAlpha( (a1 * (a2 + 1)) >> 8 );
}

byte FX::getAlpha() 
{ 
  return mLinearAlpha; 
}

bool FX::drawOn(CRGBSet dst, ulong time, ulong dt)
{
  if (mAlpha > 0) // 0 is invisible
  { 
    update(time, dt);
    memcpy8(dst.leds, mLeds, mNLEDS * sizeof(CRGB));
    
    if (mAlpha < 255) // 255 = no fade
      dst.nscale8_video(mAlpha);
    
    return true;
  }
  return false;
}

// ----------------------------------------------------
FireFX::FireFX(const bool reverse) : mReverse(reverse)
{
  mPal = HeatColors_p;
}

void FireFX::initFX()
{
  mHeat = (ushort *) malloc(mNLEDS * sizeof(ushort));
  assert (mHeat!=nullptr);

  AddVarName("speed",  mSpeed,     27,  1, 255)
  AddVarName("dim",    mDimRatio,  4,   1, 10)
}

void FireFX::update(ulong time, ulong dt)
{
  // upstream and move through z as well for changing patterns
  uint32_t YY = 15 * time * mSpeed;
  uint32_t ZZ = 3 * time * mSpeed;
  uint32_t scale = inoise16(17 * time) >> 1;
  uint32_t center = (mNLEDS / 2) - 1;
  #define NOISE(y) (inoise16(YY + scale * (y - center), ZZ) + 1)

  // seed the fire
  mHeat[mNLEDS - 1] = NOISE(0);

  // move upstream & dim
  ushort ratio = 256 / pow(mDimRatio, dt / 14.); 
	
  for (uint8_t y = 0; y < mNLEDS - 1; y++)
  {
    mHeat[y] = (mHeat[y] * 128 + mHeat[y+1] * 127) >> 8;

    uint8_t dim = 255 - (((NOISE(y + 1) >> 8) * ratio) >> 8);
    mHeat[y] = (mHeat[y] * dim) >> 8; 
  }
 
  // map the colors based on the heatmap
  for (uint8_t y = 0; y < mNLEDS; y++)
  {
    byte colorindex = scale8( mHeat[y] >> 8, 240); // scale down to 0-240 for best results with color palettes
    byte i = mReverse ?  y : mNLEDS - 1 - y;
    mLeds[i] = ColorFromPalette(mPal, colorindex);
  }
}

// ----------------------------------------------------
AquaFX::AquaFX(const bool reverse) : FireFX(reverse)
{
  mPal = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);
}

// ----------------------------------------------------
void PlasmaFX::initFX()
{
  AddVarName("p1",    mP1, 3,   1, 20)
  AddVarName("p2",    mP2, 5,   1, 20)
  AddVarName("freq",  mK,  5,   1, 20)
}

void PlasmaFX::update(ulong time, ulong dt)
{
  // cos16 & sin16(0 to 65535) => results in -32767 to 32767
  u_long t = (time * 66) >> 2;          // 65536/1000 => 2pi * time / 4
  int16_t cos_tp1 = cos16(t / mP1) >> 1;      // .5 cos(time/mP1)
  int16_t sin_tp2 = sq(sin16(t / mP2)) >> 2;  // (.5 sin(time/mP2))^2
  int16_t sin_t = sin16(t);

  int16_t x = -5215;
  int16_t step = 10430 / mNLEDS;            // 10430 = 65536 / (2 pi) => x (-.5 to .5)

  for (byte i=0; i < mNLEDS; i++, x += step)
  {
    //  cx = x + .5 cos(time/mP1); cy = .5 sin(time/mP2);
    int16_t cx = x + cos_tp1;
    int16_t sqrxy = sqrt16((cx * cx + sin_tp2) >> 16) << 8;

    // sin(time) + 2 sin(.5 (x k + time)) + sin(sqrt(k^2(cx^2 + cy^2) + 1) + time);
    int16_t v = sin_t + (sin16((mK * x + t) >> 1) << 1) + sin16(mK * sqrxy + t);
    mLeds[i] = CHSV(v >> 8, 0xff, 0xff);
  }
}

// ----------------------------------------------------
CylonFX::CylonFX(const CRGB color) : mColor(color) {}

void CylonFX::initFX()
{
  AddVarCode3("color",   mColor = CRGB(args[0], args[1], args[2]), mColor.r,   mColor.g,   mColor.b,  0, 255)
  // AddVarCode("color",   mColor = CRGB(args[0]),                   mColor,                          mColor, 0, maxCOLOR)
  AddVarCode("eyeSize", setEyeSize(args[0] * (mNLEDS - 1) / 255), mEyeSize * 255 / (mNLEDS - 1),     22,   1, 255)
  AddVarCode("speed",   mSpeed = args[0] << 3,                    mSpeed >> 3,                       3,    0, 10)
}

int CylonFX::getPos(ulong time) 
{
  return triwave8(time * mSpeed * 38 / 10000) << 8; // speed = 1<<3, 1.5 second period 
}

void CylonFX::showEye(int p)
{
  #define FRAC_SHIFT 4
  long pos16 = (ease16InOutQuad(p) * (mNLEDS - mEyeSize - 1)) >> (16-FRAC_SHIFT);
  int pos = pos16 >> FRAC_SHIFT;
  byte frac = (pos16 & 0x0F) << FRAC_SHIFT;

  mLeds[pos] = mColor % (byte)(255 - frac); // cast to avoid warning
  for (byte j = 0; j < mEyeSize; j++)
    mLeds[++pos] = mColor;
  if (pos < mNLEDS-1)
    mLeds[++pos] = mColor % frac;
}

void CylonFX::update(ulong time, ulong dt)
{
  ClearLeds(mLeds, mNLEDS)
  showEye(getPos(time));
}

// ----------------------------------------------------
void DblCylonFX::update(ulong time, ulong dt)
{
  ClearLeds(mLeds, mNLEDS)
  int pos = getPos(time);
  showEye(pos);
  showEye(65535 - pos);
}

// ----------------------------------------------------
RunningFX::RunningFX(const CRGB color, const int speed) : mSpeed(speed), mColor(color) {}

void RunningFX::initFX()
{
  AddVarCode3("color", mColor = CRGB(args[0], args[1], args[2]), mColor.r, mColor.g, mColor.b, 0, 255)
  // AddVarCode("color", mColor = CRGB(args[0]), mColor, mColor, 0, maxCOLOR)
  AddVarName("speed", mSpeed, mSpeed,   -10, 10)
  AddVarName("width", mWidth, 10,         1, 30)
}

void RunningFX::update(ulong time, ulong dt)
{
  fill_solid(mLeds, mNLEDS, mColor);

  u_long t = time * 66 * mSpeed; // 65536/1000 => 2pi * time 
  u_long x = 0; 
  u_long dx = 32768 / mWidth;
  for (byte i=0; i < mNLEDS; i++, x += dx)
  {
    int _sin = sin16(x + t);
    mLeds[i].nscale8(_sin > 0 ? _sin>>8 : 0);
  }
}

// ----------------------------------------------------
TwinkleFX::TwinkleFX(const byte hue) { setHue(hue); }

TwinkleFX::TwinkleFX(const CRGB color) { setHue(color); }

void TwinkleFX::setHue(const CRGB color)  
{  
  mHSV = rgb2hsv_approximate(color); 
  mColor = color; 
}

void TwinkleFX::setHue(const byte hue)    
{  
  mHSV = CHSV(hue, 0xff, 0xff);      
  mColor = mHSV; 
}

void TwinkleFX::initFX()
{
  AddVarCode3("color", setHue(CRGB(args[0], args[1], args[2])), mColor.r, mColor.g, mColor.b, 0, 255)
  // AddVarCode("color", setHue(CRGB(args[0])), mColor, mColor, 0, maxCOLOR)
  AddVarName("div",   mDiv, 5,   1, 20)
}

void TwinkleFX::update(ulong time, ulong dt)
{
  random16_set_seed(535);  // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

  for (int i = 0; i<mNLEDS; i++)
  {
    byte fader = sin8(time / random8(mDiv, mDiv << 1));       // The random number for each 'i' will be the same every time.
    byte hue = sin8(time / mDiv) >> 5; // ditto
    mLeds[i] = CHSV(mHSV.h + hue, mHSV.s , fader);
  }

  random16_set_seed(time); // "restart" random for other FX
}

// ----------------------------------------------------
PacificaFX::PacificaFX()
{
  mPal1 = {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50};
  mPal2 = {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
  mPal3 = {0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF};
}

void PacificaFX::initFX()
{
  AddVarName("speed", mSpeed, 4,  1, 7)
}

void PacificaFX::update(ulong time, ulong dt)
{
  dt = dt * mSpeed;
  uint32_t dt1 = (dt * beatsin16(3, 179, 269)) / 256;
  uint32_t dt2 = (dt * beatsin16(4, 179, 269)) / 256;
  uint32_t dt21 = (dt1 + dt2) / 2;
  mT1 += dt1 * beatsin88(1011, 10, 13);
  mT2 -= dt21 * beatsin88(777, 8, 11);
  mT3 -= dt1 * beatsin88(501, 5, 7);
  mT4 -= dt2 * beatsin88(257, 4, 6);

  // Clear out the LED array to a dim background blue-green
  fill_solid(mLeds, mNLEDS, CRGB(2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  oneLayer(mPal1, mT1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0-beat16(301));
  oneLayer(mPal2, mT2, beatsin16(4, 6 * 256,  9 * 256), beatsin8(17, 40,  80), beat16(401));
  oneLayer(mPal3, mT3, 6 * 256, beatsin8(9, 10, 38), 0-beat16(503));
  oneLayer(mPal3, mT4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  uint8_t basethreshold = beatsin8(9, 55, 65);
  uint8_t wave = beat8(7);
  
  for (byte i = 0; i < mNLEDS; i++)
  {
    uint8_t threshold = scale8(sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = mLeds[i].getAverageLight();
    if( l > threshold)
    {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8(overage, overage);
      mLeds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
    }
  }

  // Deepen the blues and greens a bit
  for (byte i = 0; i < mNLEDS; i++)
  {
    mLeds[i].blue = scale8(mLeds[i].blue, 145); 
    mLeds[i].green = scale8(mLeds[i].green, 200); 
    mLeds[i] |= CRGB(2, 5, 7);
  }
}

// Add one layer of waves into the led array
void PacificaFX::oneLayer(CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t waveangle)
{
  uint16_t wavescaleHalf = (wavescale / 2) + 20;
  for (byte i = 0; i < mNLEDS; i++)
  {
    waveangle += 250;
    uint16_t s16 = sin16(waveangle) + 32768;
    uint16_t cs = scale16(s16 , wavescaleHalf) + wavescaleHalf;
    cistart += cs;
    uint16_t sindex16 = sin16(cistart) + 32768;
    uint8_t sindex8 = scale16(sindex16, 240);
    mLeds[i] += ColorFromPalette(p, sindex8, bri, LINEARBLEND);
  }
}
