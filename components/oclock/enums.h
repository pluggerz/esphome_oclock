#pragma once

// Set the number of LEDs to control.
constexpr int LED_COUNT = 12;

namespace oclock
{
    enum class ForegroundEnum
    {
        First = 0,
        None = First,
        DebugLeds,
        FollowHandles,
        SolidColorSelector,
        RgbColors,
    };

    enum class BackgroundEnum
    {
        First = 0,
        SolidColor = First,
        WarmWhiteShimmer,
        RandomColorWalk,
        TraditionalColors,
        ColorExplosion,
        Gradient,
        BrightTwinkle,
        Collision,
        Rainbow,
        RgbColors, // will be SolidColor
    };

    enum class InBetweenAnimationEnum
    {
        Random,
        None,
        Star,
        Dash,
        Middle1,
        Middle2,
        PacMan,
    };

    enum class HandlesDistanceEnum
    {
        Random,
        Shortest,
        Left,
        Right,
    };

    enum class HandlesAnimationEnum
    {
        Random,
        Swipe,
        Distance,
    };

    enum class ActiveMode
    {
        None,
        TrackHassioTime,
        TrackInternalTime,
        TrackTestTime,
    };

    union RgbColor16
    {
        uint16_t raw;
        struct
        {
            uint16_t red : 5,
                green : 5,
                blue : 5,
                alpha : 1;
        } rgb;
    } __attribute__((packed, aligned(1)));

    const int ALPHA_BITS = 3;
    const int BRIGHTNESS_BITS = 5;
    const int MAX_ALPHA = (1 << ALPHA_BITS) - 1;
    const int MAX_BRIGHTNESS = (1 << BRIGHTNESS_BITS) - 1;

    struct RgbColor
    {
    private: // TODO: should be private
        uint8_t red, green, blue;
        uint8_t alpha : ALPHA_BITS, brightness : BRIGHTNESS_BITS;

    public:
        RgbColor() : red(0), green(0), blue(0), alpha(0), brightness(MAX_BRIGHTNESS) {}
        RgbColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0, uint8_t br = MAX_BRIGHTNESS)
            : red(r),
              green(g),
              blue(b),
              alpha(a),
              brightness(br) {}

        uint8_t get_alpha() const { return alpha; }
        void set_alpha(uint8_t value) { alpha = value; }

        uint8_t get_brightness() const { return brightness; }

        uint8_t get_red() const { return red; }
        void set_red(uint8_t value) { red = value; }

        uint8_t get_green() const { return green; }
        void set_green(uint8_t value) { green = value; }

        uint8_t get_blue() const { return blue; }
        void set_blue(uint8_t value) { blue = value; }

        bool invisible() const { return alpha == MAX_ALPHA; }

        friend bool operator==(const RgbColor &l, const RgbColor &r)
        {
            return l.red == r.red && l.green == r.green && l.blue == r.blue && l.alpha == r.alpha && l.brightness == r.brightness;
        }
        friend bool operator!=(const RgbColor &c1, const RgbColor &c2)
        {
            return !(c1 == c2);
        }

    private:
#ifdef ESP8266
        // based on https://gist.github.com/fairlight1337/4935ae72bcbcc1ba5c72

        /*! \brief Convert RGB to HSV color space

          Converts a given set of RGB values `r', `g', `b' into HSV
          coordinates. The input RGB values are in the range [0, 1], and the
          output HSV values are in the ranges h = [0, 360], and s, v = [0,
          1], respectively.

          \param fR Red component, used as input, range: [0, 1]
          \param fG Green component, used as input, range: [0, 1]
          \param fB Blue component, used as input, range: [0, 1]
          \param fH Hue component, used as output, range: [0, 360]
          \param fS Hue component, used as output, range: [0, 1]
          \param fV Hue component, used as output, range: [0, 1]

        */
        static void RGBtoHSV(const float &fR, const float &fG, const float fB, float &fH, float &fS, float &fV)
        {
            float fCMax = max(max(fR, fG), fB);
            float fCMin = min(min(fR, fG), fB);
            float fDelta = fCMax - fCMin;

            if (fDelta > 0)
            {
                if (fCMax == fR)
                {
                    fH = 60 * (fmod(((fG - fB) / fDelta), 6));
                }
                else if (fCMax == fG)
                {
                    fH = 60 * (((fB - fR) / fDelta) + 2);
                }
                else if (fCMax == fB)
                {
                    fH = 60 * (((fR - fG) / fDelta) + 4);
                }

                if (fCMax > 0)
                {
                    fS = fDelta / fCMax;
                }
                else
                {
                    fS = 0;
                }

                fV = fCMax;
            }
            else
            {
                fH = 0;
                fS = 0;
                fV = fCMax;
            }

            if (fH < 0)
            {
                fH = 360 + fH;
            }
        }

        /*! \brief Convert HSV to RGB color space

          Converts a given set of HSV values `h', `s', `v' into RGB
          coordinates. The output RGB values are in the range [0, 1], and
          the input HSV values are in the ranges h = [0, 360], and s, v =
          [0, 1], respectively.

          \param fR Red component, used as output, range: [0, 1]
          \param fG Green component, used as output, range: [0, 1]
          \param fB Blue component, used as output, range: [0, 1]
          \param fH Hue component, used as input, range: [0, 360]
          \param fS Hue component, used as input, range: [0, 1]
          \param fV Hue component, used as input, range: [0, 1]

        */
        static void HSVtoRGB(float &fR, float &fG, float &fB, const float &fH, const float &fS, const float &fV)
        {
            float fC = fV * fS; // Chroma
            float fHPrime = fmod(fH / 60.0, 6);
            float fX = fC * (1 - fabs(fmod(fHPrime, 2) - 1));
            float fM = fV - fC;

            if (0 <= fHPrime && fHPrime < 1)
            {
                fR = fC;
                fG = fX;
                fB = 0;
            }
            else if (1 <= fHPrime && fHPrime < 2)
            {
                fR = fX;
                fG = fC;
                fB = 0;
            }
            else if (2 <= fHPrime && fHPrime < 3)
            {
                fR = 0;
                fG = fC;
                fB = fX;
            }
            else if (3 <= fHPrime && fHPrime < 4)
            {
                fR = 0;
                fG = fX;
                fB = fC;
            }
            else if (4 <= fHPrime && fHPrime < 5)
            {
                fR = fX;
                fG = 0;
                fB = fC;
            }
            else if (5 <= fHPrime && fHPrime < 6)
            {
                fR = fC;
                fG = 0;
                fB = fX;
            }
            else
            {
                fR = 0;
                fG = 0;
                fB = 0;
            }

            fR += fM;
            fG += fM;
            fB += fM;
        }

    public:
        static RgbColor h_to_rgb(float H)
        {
            while (H < 0)
                H += 360.0;

            while (H > 360.0)
                H -= 360.0;

            return HSVtoRGB(H, 1., 1.);
        }

        static RgbColor HSVtoRGB(float h, float s, float v)
        {
            float r, g, b;
            HSVtoRGB(r, g, b, h, s, v);
            return RgbColor(r * 255, g * 255, b * 255);
        }

        int as_h() const
        {
            float r = red / 255., g = green / 255., b = blue / 255.;
            float h = 0.0, s = 0.0, v = 0.0;
            RGBtoHSV(r, g, b, h, s, v);
            return h;
        }
#endif
    } __attribute__((packed, aligned(1)));

    typedef RgbColor RgbColorLeds[LED_COUNT];
}