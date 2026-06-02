// Le bloc ifdef suivant est la façon standard de créer des macros qui facilitent l'exportation
// à partir d'une DLL plus simple. Tous les fichiers contenus dans cette DLL sont compilés avec le symbole QPVMAIN_EXPORTS
// défini sur la ligne de commande. Ce symbole ne doit pas être défini pour un projet
// qui utilise cette DLL. Ainsi, les autres projets dont les fichiers sources comprennent ce fichier considèrent les fonctions
// QPVMAIN_API comme étant importées à partir d'une DLL, tandis que cette DLL considère les symboles
// définis avec cette macro comme étant exportés.

const DWORD dwHwndTabletProperty = 
    TABLET_DISABLE_PRESSANDHOLD |      // disables press and hold (right-click) gesture
    TABLET_DISABLE_PENTAPFEEDBACK |    // disables UI feedback on pen up (waves)
    TABLET_DISABLE_PENBARRELFEEDBACK | // disables UI feedback on pen button down (circle)
    TABLET_DISABLE_FLICKFALLBACKKEYS |
    TABLET_DISABLE_SMOOTHSCROLLING |
    TABLET_DISABLE_TOUCHUIFORCEON |
    TABLET_DISABLE_FLICKS;             // disables pen flicks (back, forward, drag down, drag up)

const double M_PI = 3.14159265358979323846;  // PI
const double div2sz = sqrt(2.0 / 32.0);      // used in calculateDCT()
const double div2sq = 1 / sqrt(2.0);         // used in calculateDCT()
const float div2s3 = 2.0f/3.0f;      // used in ConvertRGBtoHSL()
const float div1s3 = 1.0f/3.0f;      // used in ConvertRGBtoHSL()
float imgSelExclW = 0.0f;
float imgSelExclH = 0.0f;
float imgSelExclX = 0.0f;
float imgSelExclY = 0.0f;
int imgSelX1 = 0;
int imgSelY1 = 0;
int imgSelX2 = 0;
int imgSelY2 = 0;
int imgSelW = 0;
int imgSelH = 0;
int EllipseSelectMode = 0;
int flippedSelection = 0;
int invertSelection = 0;
int highDepthModeMask = 0;
float excludeSelectScale = 0;
float vpSelRotation = 0;
float cosVPselRotation = 0;
float sinVPselRotation = 0;
float hImgSelW = 0.0f;
float hImgSelH = 0.0f;
float imgSelXscale = 0.0f;
float imgSelYscale = 0.0f;
INT64 polyW = 0;
INT64 polyH = 0;
INT64 polyX = 0;
INT64 polyY = 0;
INT64 polyOffYa = 0;
INT64 polyOffYb = 0;
INT64 blahImgH = 0;

std::vector<float*> brushOpacityChunks;
int chunkGridW = 0;
int chunkGridH = 0;

IWICBitmapDecoder      *pWICclassDecoder;
IWICBitmapFrameDecode  *pWICclassFrameDecoded;
// IWICFormatConverter *pWICclassConverter;
IWICBitmapSource       *pWICclassPixelsBitmapSource;

class MaskBitMap {
private:
    std::vector<uint64_t> data;
    size_t num_bits = 0;

public:
    void resize(size_t size) {
        num_bits = size;
        data.assign((size + 63) / 64, 0ULL);
    }

    void clear() {
        data.clear();
        num_bits = 0;
    }

    void shrink_to_fit() {
        data.shrink_to_fit();
    }

    size_t size() const {
        return num_bits;
    }

    struct Reference {
        uint64_t* word;
        uint64_t mask;

        Reference(uint64_t* w, uint64_t m) : word(w), mask(m) {}

        Reference& operator=(bool val) {
            auto* atomic_word = reinterpret_cast<std::atomic<uint64_t>*>(word);
            if (val) {
                atomic_word->fetch_or(mask, std::memory_order_relaxed);
            } else {
                atomic_word->fetch_and(~mask, std::memory_order_relaxed);
            }
            return *this;
        }

        Reference& operator=(const Reference& other) {
            return operator=(bool(other));
        }

        operator bool() const {
            return (*word & mask) != 0;
        }
    };

    Reference operator[](size_t idx) {
        return Reference(&data[idx / 64], 1ULL << (idx % 64));
    }

    bool operator[](size_t idx) const {
        return (data[idx / 64] & (1ULL << (idx % 64))) != 0;
    }

    void fill_zero() {
        std::fill(data.begin(), data.end(), 0ULL);
    }

    void fill_zero(size_t start, size_t end) {
        if (start >= end) return;
        size_t start_word = start / 64;
        size_t end_word = (end - 1) / 64;

        if (start_word == end_word) {
            uint64_t mask = (~0ULL << (start % 64)) & (~0ULL >> (63 - ((end - 1) % 64)));
            auto* atomic_word = reinterpret_cast<std::atomic<uint64_t>*>(&data[start_word]);
            atomic_word->fetch_and(~mask, std::memory_order_relaxed);
        } else {
            // First word (partial)
            uint64_t start_mask = (~0ULL << (start % 64));
            reinterpret_cast<std::atomic<uint64_t>*>(&data[start_word])->fetch_and(~start_mask, std::memory_order_relaxed);

            // Middle words (full)
            for (size_t w = start_word + 1; w < end_word; ++w) {
                data[w] = 0ULL;
            }

            // Last word (partial)
            uint64_t end_mask = (~0ULL >> (63 - ((end - 1) % 64)));
            reinterpret_cast<std::atomic<uint64_t>*>(&data[end_word])->fetch_and(~end_mask, std::memory_order_relaxed);
        }
    }

    void set_range_to_1(size_t start, size_t end) {
        if (start > end) return;
        size_t start_word = start / 64;
        size_t end_word = end / 64;

        if (start_word == end_word) {
            uint64_t mask = (~0ULL << (start % 64)) & (~0ULL >> (63 - (end % 64)));
            auto* atomic_word = reinterpret_cast<std::atomic<uint64_t>*>(&data[start_word]);
            atomic_word->fetch_or(mask, std::memory_order_relaxed);
        } else {
            // First word (partial)
            uint64_t start_mask = (~0ULL << (start % 64));
            reinterpret_cast<std::atomic<uint64_t>*>(&data[start_word])->fetch_or(start_mask, std::memory_order_relaxed);

            // Middle words (full)
            for (size_t w = start_word + 1; w < end_word; ++w) {
                data[w] = ~0ULL;
            }

            // Last word (partial)
            uint64_t end_mask = (~0ULL >> (63 - (end % 64)));
            reinterpret_cast<std::atomic<uint64_t>*>(&data[end_word])->fetch_or(end_mask, std::memory_order_relaxed);
        }
    }
};

std::vector<unsigned char>  highDephMaskMap;
MaskBitMap  polygonMaskMap;
MaskBitMap  polygonOtherMaskMap;
// std::vector<std::vector<short>> DrawLineCapsGrid;
vector<pair<float, float>> DrawLineCapsGrid;
// vector<pair<int, int>> DrawLineGrid;

std::vector<UINT>  dupesListIDsA(1);
std::vector<UINT>  dupesListIDsB(1);
std::vector<UINT>  dupesListIDsC(1);
// std::unordered_map<UINT, unsigned char>  brushMoveImgData(1);

std::array<double, 1025>  DCTcoeffs;

struct GUIDComparer {
    bool operator()(const GUID& left, const GUID& right) const {
        return memcmp(&left, &right, sizeof(GUID)) < 0;
    }
};

struct Point {
    double x, y;
};

struct RGBColor {
    double r, g, b;
};

struct RGBColorI {
    int r, g, b;
};

struct HSLColor {
    double h, s, l;

    double inline ConvertHueToRGB(double v1, double v2, double vH) {
        if (vH < 0.0) vH += 1.0;
        if (vH > 1.0) vH -= 1.0;
        if (6.0 * vH < 1.0) return v1 + (v2 - v1) * 6.0 * vH;
        if (2.0 * vH < 1.0) return v2;
        if (3.0 * vH < 2.0) return v1 + (v2 - v1) * (div2s3 - vH) * 6.0;
        return v1;
    }

    RGBColorI ConvertHSLtoRGB() {
        double fH = h / 360.0;
        double var_1, var_2;
        RGBColorI newColor;
        if (s <= 0.0)
        {
            newColor.r = clamp((float)(l * 255.0), 0.0f, 255.0f);
            newColor.g = clamp((float)(l * 255.0), 0.0f, 255.0f);
            newColor.b = clamp((float)(l * 255.0), 0.0f, 255.0f);
        } else
        {
            if (l < 0.5)
                var_2 = l * (1.0 + s);
            else
                var_2 = (l + s) - (s * l);

            var_1 = 2.0 * l - var_2;
            newColor.r = clamp((float)(255.0 * ConvertHueToRGB(var_1, var_2, fH + div1s3)), 0.0f, 255.0f);
            newColor.g = clamp((float)(255.0 * ConvertHueToRGB(var_1, var_2, fH)), 0.0f, 255.0f);
            newColor.b = clamp((float)(255.0 * ConvertHueToRGB(var_1, var_2, fH - div1s3)), 0.0f, 255.0f);
        }
        return newColor;
    }

    RGBColorI ConvertHSLtoRGBint16() {
        const double fH = h / 360.0;
        double var_1, var_2;
        RGBColorI newColor;
        if (s <= 0.0)
        {
            newColor.r = clamp((float)(l * 65535.0), 0.0f, 65535.0f);
            newColor.g = clamp((float)(l * 65535.0), 0.0f, 65535.0f);
            newColor.b = clamp((float)(l * 65535.0), 0.0f, 65535.0f);
        } else
        {
            if (l < 0.5)
                var_2 = l * (1.0 + s);
            else
                var_2 = (l + s) - (s * l);

            var_1 = 2.0 * l - var_2;
            newColor.r = clamp((float)(65535.0 * ConvertHueToRGB(var_1, var_2, fH + div1s3)), 0.0f, 65535.0f);
            newColor.g = clamp((float)(65535.0 * ConvertHueToRGB(var_1, var_2, fH)), 0.0f, 65535.0f);
            newColor.b = clamp((float)(65535.0 * ConvertHueToRGB(var_1, var_2, fH - div1s3)), 0.0f, 65535.0f);
        }
        return newColor;
    }
};

struct RGBAColor {
    int b, g, r, a;

    HSLColor ConvertRGBtoHSL() {
        const double rf = clamp(r, 0, 255) / 255.0;
        const double gf = clamp(g, 0, 255) / 255.0;
        const double bf = clamp(b, 0, 255) / 255.0;
        const double minu    = min(rf, min(gf, bf));
        const double maxu    = max(rf, max(gf, bf));
        const double del_Max = maxu - minu;
        const double L       = (maxu + minu) / 2.0;
        double H = 0.0, S = 0.0;

        if (del_Max > 0.0)
        {
            if (L < 0.5)
                S = del_Max / (maxu + minu);
            else
                S = del_Max / (2.0 - del_Max);

            const double del_R = (((maxu - rf) / 6.0) + (del_Max / 2.0)) / del_Max;
            const double del_G = (((maxu - gf) / 6.0) + (del_Max / 2.0)) / del_Max;
            const double del_B = (((maxu - bf) / 6.0) + (del_Max / 2.0)) / del_Max;

            if (rf == maxu)
                H = del_B - del_G;
            else if (gf == maxu)
                H = div1s3 + del_R - del_B;
            else
                H = div2s3 + del_G - del_R;

            if (H < 0.0)
                H += 1.0;
            if (H > 1.0)
                H -= 1.0;
        }

        return {H * 360.0, S, L};
    }

    void channelOffset(int ao, int ro, int go, int bo) {
        a = clamp(a + ao, 0, 255);
        r = clamp(r + ro, 0, 255);
        g = clamp(g + go, 0, 255);
        b = clamp(b + bo, 0, 255);
    }

    void threshold(int ao, int ro, int go, int bo, int seeThrough) {
        if (seeThrough == 2)
        {
            if (ao >= 0) a = (a > ao) ? a : 0;
            if (ro >= 0) r = (r > ro) ? r : 0;
            if (go >= 0) g = (g > go) ? g : 0;
            if (bo >= 0) b = (b > bo) ? b : 0;
        } else if (seeThrough == 3)
        {
            if (ao >= 0) a = (a > ao) ? 255 : a;
            if (ro >= 0) r = (r > ro) ? 255 : r;
            if (go >= 0) g = (g > go) ? 255 : g;
            if (bo >= 0) b = (b > bo) ? 255 : b;
        } else
        {
            if (ao >= 0) a = (a > ao) ? 255 : 0;
            if (ro >= 0) r = (r > ro) ? 255 : 0;
            if (go >= 0) g = (g > go) ? 255 : 0;
            if (bo >= 0) b = (b > bo) ? 255 : 0;
        }
    }

    void invert() {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;
    }

    void blackPoint(int level, int noise) {
        int rando = 0;
        if (noise == 1) {
            thread_local unsigned int seed = 123456789U;
            seed = seed * 1103515245U + 12345U;
            rando = (int)((seed / 65536U) % 10U);
        }
        r = max(r, level + rando);
        g = max(g, level + rando);
        b = max(b, level + rando);
    }

    void whitePoint(int level, int noise) {
        int rando = 0;
        if (noise == 1) {
            thread_local unsigned int seed = 123456789U;
            seed = seed * 1103515245U + 12345U;
            rando = (int)((seed / 65536U) % 10U);
        }
        r = min(r, level - rando);
        g = min(g, level - rando);
        b = min(b, level - rando);
    }

    void brightness(int level, int altMode) {
        if (altMode == 0)
        {
            if (level < 0)
            {
                r = LUTgammaBright[clamp(r, 0, 255)];
                g = LUTgammaBright[clamp(g, 0, 255)];
                b = LUTgammaBright[clamp(b, 0, 255)];
            }
            r = clamp(r + level, 0, 255);
            g = clamp(g + level, 0, 255);
            b = clamp(b + level, 0, 255);
        } else
        {
            r = LUTbright[clamp(r, 0, 255)];
            g = LUTbright[clamp(g, 0, 255)];
            b = LUTbright[clamp(b, 0, 255)];
        }
    }

    void shadows(int altMode, int linearGamma, int gray) {
        int nr, ng, nb;
        if (altMode == 1)
            gray = clamp(255 - gray, 0, 255);
        else
            gray = clamp(255 - (gray * 2), 0, 255);

        nr = LUTshadows[clamp(r, 0, 255)];
        ng = LUTshadows[clamp(g, 0, 255)];
        nb = LUTshadows[clamp(b, 0, 255)];
        float fintensity = clamp(gray, 0, 255) / 255.0f;
        if (linearGamma == 1)
        {
            fintensity += 0.1f;
            r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nr, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
            g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(ng, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
            b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nb, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
        } else
        {
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        }
    }

    void highlights(int altMode, int linearGamma, float factor, int gray) {
        int nr, ng, nb;
        if (altMode == 1)
            gray = contraMaths(gray * 1.5f, factor, 128);
        else
            gray = contraMaths(gray, factor, 128);

        nr = LUThighs[clamp(r, 0, 255)];
        ng = LUThighs[clamp(g, 0, 255)];
        nb = LUThighs[clamp(b, 0, 255)];
        float fintensity = clamp(gray, 0, 255) / 255.0f;
        if (linearGamma == 1)
        {
            r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nr, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
            g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(ng, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
            b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nb, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
        } else
        {
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        }
    }

    void gamma() {
        r = LUTgamma[clamp(r, 0, 255)];
        g = LUTgamma[clamp(g, 0, 255)];
        b = LUTgamma[clamp(b, 0, 255)];
    }

    void contrast(int level, int altContra, int linearGamma, float fintensity) {
        if (altContra == 1)
        {
            a = LUTcontra[clamp(a, 0, 255)];
            return;
        }

        if (level > 0)
        {
            int gray = getGrayscale(r, g, b);
            if (linearGamma == 1)
            {
                r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
                g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
                b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
            } else
            {
                r = weighTwoValues(gray, r, fintensity);
                g = weighTwoValues(gray, g, fintensity);
                b = weighTwoValues(gray, b, fintensity);
            }
        }

        r = LUTcontra[clamp(r, 0, 255)];
        g = LUTcontra[clamp(g, 0, 255)];
        b = LUTcontra[clamp(b, 0, 255)];
    }

    void saturation(int level, int altMode, int linearGamma, float saturation) {
        if (altMode > 1)
        {
            int gray = (altMode == 2) ? r : g;
            if (altMode == 3)
                gray = b;
            r = gray;
            g = gray;
            b = gray;
        } else if (altMode == 1)
        {
            HSLColor HSLu = ConvertRGBtoHSL();
            saturation = (level < 0) ? 0.001f : saturation;
            HSLColor newHSL = {HSLu.h, saturation, HSLu.l};
            RGBColorI newRGB = newHSL.ConvertHSLtoRGB();
            float fi = 0.0f;
            if (inRange(0, 16384, level))
                fi = level / 16384.0f;
            else if (inRange(-65535, 0, level))
                fi = abs(level) / 65535.0f;

            if (inRange(-65535, 16384, level))
            {
                r = weighTwoValues(newRGB.r, r, fi);
                g = weighTwoValues(newRGB.g, g, fi);
                b = weighTwoValues(newRGB.b, b, fi);
            } else
            {
                r = newRGB.r;
                g = newRGB.g;
                b = newRGB.b;
            }
        } else if (level < 0)
        {
            const int gray = getGrayscale(r, g, b);
            float fintensity = clamp(abs(level), 0, 65535) / 65535.0f;
            if (linearGamma == 1)
            {
                r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
                g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
                b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(gray, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
            } else
            {
                r = weighTwoValues(gray, r, fintensity);
                g = weighTwoValues(gray, g, fintensity);
                b = weighTwoValues(gray, b, fintensity);
            }
        } else
        {
            const float max_val = max(max(r, g), b);
            const float min_val = min(min(r, g), b);
            const float luxAvg = (max_val + min_val) / 2.0f;
            const float factor = (level + 21823) / 21823.0f;

            float lux = clamp(getGrayscale(r, g, b) / 3.0f, 0.0f, 255.0f);
            lux = weighTwoValues(lux, 0.0f, clamp(level, 0, 65535) / 65535.0f);

            r = clamp((int)round(factor * ((float)r - luxAvg) + luxAvg + lux), 0, 255);
            g = clamp((int)round(factor * ((float)g - luxAvg) + luxAvg + lux), 0, 255);
            b = clamp((int)round(factor * ((float)b - luxAvg) + luxAvg + lux), 0, 255);
        }
    }

    void hueRotate(int degrees, float saturation, int altMode, int level) {
        HSLColor HSLu = ConvertRGBtoHSL();
        float hue = HSLu.h + (float)degrees;
        while (hue > 360.0f) hue -= 360.0f;
        while (hue < 0.0f) hue += 360.0f;

        HSLColor newHSL = {hue, HSLu.s + 0.01, HSLu.l};
        RGBColorI newRGB = newHSL.ConvertHSLtoRGB();
        float fi = 0.0f;
        if (inRange(0, 15, degrees))
            fi = degrees / 15.0f;
        else if (inRange(-15, 0, degrees))
            fi = abs(degrees) / 15.0f;

        if (inRange(-15, 15, degrees))
        {
            r = weighTwoValues(newRGB.r, r, fi);
            g = weighTwoValues(newRGB.g, g, fi);
            b = weighTwoValues(newRGB.b, b, fi);
        } else
        {
            r = newRGB.r;
            g = newRGB.g;
            b = newRGB.b;
        }
    }

    void tinto(int degrees, int level, int linearGamma) {
        HSLColor HSLu = ConvertRGBtoHSL();
        HSLColor newHSL = {(double)degrees, 0.5, HSLu.l};
        RGBColorI newRGB = newHSL.ConvertHSLtoRGB();
        const float fintensity = clamp(level, 0, 65535) / 65535.0f;
        if (linearGamma == 1)
        {
            r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(newRGB.r, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
            g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(newRGB.g, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
            b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(newRGB.b, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
        } else
        {
            r = weighTwoValues(newRGB.r, r, fintensity);
            g = weighTwoValues(newRGB.g, g, fintensity);
            b = weighTwoValues(newRGB.b, b, fintensity);
        }
    }

    void tint(float hue, int level, int altMode, int linearGamma) {
        if (altMode == 1)
            return tinto(hue, level, linearGamma);

        int z = getGrayscale(r, g, b);
        float gray = z / 255.0f;
        float normalized_hue = hue;
        while (normalized_hue > 360.0f) normalized_hue -= 360.0f;
        while (normalized_hue < 0.0f) normalized_hue += 360.0f;
        const int hi = (int)(floor(normalized_hue / 60.0f)) % 6;
        const float f = normalized_hue / 60.0f - floor(normalized_hue / 60.0f);
        const float q = gray * (1.0f - f);
        const float t = gray * (1.0f - (1.0f - f));
        int nr = 0, ng = 0, nb = 0;
        switch (hi) {
            case 0:
                nr = z;
                ng = (int)round(t * 255.0f);
                nb = 0;
                break;
            case 1:
                nr = (int)round(q * 255.0f);
                ng = z;
                nb = 0;
                break;
            case 2:
                nr = 0;
                ng = z;
                nb = (int)round(t * 255.0f);
                break;
            case 3:
                nr = 0;
                ng = (int)round(q * 255.0f);
                nb = z;
                break;
            case 4:
                nr = (int)round(t * 255.0f);
                ng = 0;
                nb = z;
                break;
            case 5:
                nr = z;
                ng = 0;
                nb = (int)round(q * 255.0f);
                break;
        }

        z = z / 3;
        const float fintensity = clamp(level, 0, 65535) / 65535.0f;
        nr = clamp(nr + z, 0, 255);
        ng = clamp(ng + z, 0, 255);
        nb = clamp(nb + z, 0, 255);
        if (linearGamma == 1)
        {
            r = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nr, 0, 255)], gamma_to_linear[clamp(r, 0, 255)], fintensity), 0, 32768)];
            g = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(ng, 0, 255)], gamma_to_linear[clamp(g, 0, 255)], fintensity), 0, 32768)];
            b = linear_to_gamma[clamp(weighTwoValues(gamma_to_linear[clamp(nb, 0, 255)], gamma_to_linear[clamp(b, 0, 255)], fintensity), 0, 32768)];
        } else
        {
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        }
struct RGBA16color {
    int b, g, r, a;

    HSLColor ConvertRGBtoHSL() {
        const double rf = clamp(r, 0, 65535) / 65535.0;
        const double gf = clamp(g, 0, 65535) / 65535.0;
        const double bf = clamp(b, 0, 65535) / 65535.0;
        const double minu    = min(rf, min(gf, bf));
        const double maxu    = max(rf, max(gf, bf));
        const double del_Max = maxu - minu;
        const double L       = (maxu + minu) / 2.0;
        double H = 0.0, S = 0.0;

        if (del_Max > 0.0)
        {
            if (L < 0.5)
                S = del_Max / (maxu + minu);
            else
                S = del_Max / (2.0 - del_Max);

            const double del_R = (((maxu - rf) / 6.0) + (del_Max / 2.0)) / del_Max;
            const double del_G = (((maxu - gf) / 6.0) + (del_Max / 2.0)) / del_Max;
            const double del_B = (((maxu - bf) / 6.0) + (del_Max / 2.0)) / del_Max;

            if (rf == maxu)
                H = del_B - del_G;
            else if (gf == maxu)
                H = div1s3 + del_R - del_B;
            else
                H = div2s3 + del_G - del_R;

            if (H < 0.0)
                H += 1.0;
            if (H > 1.0)
                H -= 1.0;
        }

        return {H * 360.0, S, L};
    }

    void channelOffset(int ao, int ro, int go, int bo, int noClamping) {
        a = clamp(a + ao, 0, 65535);
        r = (noClamping == 1) ? r + ro : clamp(r + ro, 0, 65535);
        g = (noClamping == 1) ? g + go : clamp(g + go, 0, 65535);
        b = (noClamping == 1) ? b + bo : clamp(b + bo, 0, 65535);
    }

    void threshold(int ao, int ro, int go, int bo, int seeThrough) {
        if (seeThrough == 2)
        {
            if (ao >= 0) a = (a > ao) ? a : 0;
            if (ro >= 0) r = (r > ro) ? r : 0;
            if (go >= 0) g = (g > go) ? g : 0;
            if (bo >= 0) b = (b > bo) ? b : 0;
        } else if (seeThrough == 3)
        {
            if (ao >= 0) a = (a > ao) ? 65535 : a;
            if (ro >= 0) r = (r > ro) ? 65535 : r;
            if (go >= 0) g = (g > go) ? 65535 : g;
            if (bo >= 0) b = (b > bo) ? 65535 : b;
        } else
        {
            if (ao >= 0) a = (a > ao) ? 65535 : 0;
            if (ro >= 0) r = (r > ro) ? 65535 : 0;
            if (go >= 0) g = (g > go) ? 65535 : 0;
            if (bo >= 0) b = (b > bo) ? 65535 : 0;
        }
    }

    void invert() {
        r = 65535 - r;
        g = 65535 - g;
        b = 65535 - b;
    }

    void blackPoint(int level, int noise) {
        int rando = 0;
        if (noise == 1) {
            thread_local unsigned int seed = 123456789U;
            seed = seed * 1103515245U + 12345U;
            rando = (int)((seed / 65536U) % 2600U);
        }
        r = max(r, level + rando);
        g = max(g, level + rando);
        b = max(b, level + rando);
    }

    void whitePoint(int level, int noise) {
        int rando = 0;
        if (noise == 1) {
            thread_local unsigned int seed = 123456789U;
            seed = seed * 1103515245U + 12345U;
            rando = (int)((seed / 65536U) % 2600U);
        }
        r = min(r, level - rando);
        g = min(g, level - rando);
        b = min(b, level - rando);
    }

    void brightness(int level, int altMode, int noClamping, float fintensity) {
        if (altMode == 0)
        {
            if (level < 0 && noClamping == 0)
            {
                r = LUTgammaBright[clamp(r, 0, 65535)];
                g = LUTgammaBright[clamp(g, 0, 65535)];
                b = LUTgammaBright[clamp(b, 0, 65535)];
            }
            r = (noClamping == 1) ? r + level : clamp(r + level, 0, 65535);
            g = (noClamping == 1) ? g + level : clamp(g + level, 0, 65535);
            b = (noClamping == 1) ? b + level : clamp(b + level, 0, 65535);
        } else
        {
            if (noClamping == 1)
            {
                r = r + (float)r * fintensity;
                g = g + (float)g * fintensity;
                b = b + (float)b * fintensity;
            } else
            {
                r = LUTbright[clamp(r, 0, 65535)];
                g = LUTbright[clamp(g, 0, 65535)];
                b = LUTbright[clamp(b, 0, 65535)];
            }
        }
    }

    int getGrayscaleAdvanced() {
        const float minu = min((float)r, min((float)g, (float)b));
        float maxu = max((float)r, max((float)g, (float)b));
        float nr = r;
        float ng = g;
        float nb = b;
        if (minu < 0.0f)
        {
            nr = r - minu;
            ng = g - minu;
            nb = b - minu;
        }
        if (maxu < 65535.0f)
            maxu = 65535.0f;

        nr = nr * 0.299701f;
        ng = ng * 0.587130f;
        nb = nb * 0.114180f;
        int gray = (int)round(nr + ng + nb);
        if ((float)gray > maxu)
            gray = (int)maxu;

        return gray;
    }

    void shadows(int level, int altMode, int linearGamma, int gray, int noClamping, float fi) {
        int nr, ng, nb;
        if (noClamping == 1)
        {
            float maxu = max((float)r, max((float)g, (float)b));
            if (maxu < 65535.0f)
                maxu = 65535.0f;

            nr = r + (float)r * fi;
            ng = g + (float)g * fi;
            nb = b + (float)b * fi;
            float gz = getGrayscaleAdvanced();
            if (altMode != 1)
                gz = gz * 2.0f;

            float fintensity = 1.0f - (gz / maxu);
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        } else
        {
            if (altMode == 1)
                gray = clamp(65535 - gray, 0, 65535);
            else
                gray = clamp(65535 - (gray * 2), 0, 65535);

            nr = LUTshadows[clamp(r, 0, 65535)];
            ng = LUTshadows[clamp(g, 0, 65535)];
            nb = LUTshadows[clamp(b, 0, 65535)];
            float fintensity = clamp(gray, 0, 65535) / 65535.0f;
            if (linearGamma == 1)
            {
                fintensity += 0.1f;
                r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nr, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
                g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(ng, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
                b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nb, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
            } else
            {
                r = weighTwoValues(nr, r, fintensity);
                g = weighTwoValues(ng, g, fintensity);
                b = weighTwoValues(nb, b, fintensity);
            }
        }
    }

    void highlights(int level, int altMode, int linearGamma, float factor, int gray, int noClamping, float fi) {
        int nr, ng, nb;
        if (noClamping == 1)
        {
            float maxu = max((float)r, max((float)g, (float)b));
            if (maxu < 65535.0f)
                maxu = 65535.0f;

            nr = r + (float)r * fi;
            ng = g + (float)g * fi;
            nb = b + (float)b * fi;
            float gz = getGrayscaleAdvanced();
            if (altMode == 1)
                gz = gz * 1.25f;
            else
                gz = gz / 1.25f;

            float fintensity = gz / (maxu / 1.5f);
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        } else
        {
            if (altMode == 1)
                gray = contraMathsInt16(gray * 1.5f, factor, 32768);
            else
                gray = contraMathsInt16(gray, factor, 32768);

            nr = LUThighs[clamp(r, 0, 65535)];
            ng = LUThighs[clamp(g, 0, 65535)];
            nb = LUThighs[clamp(b, 0, 65535)];
            float fintensity = clamp(gray, 0, 65535) / 65535.0f;
            if (linearGamma == 1)
            {
                r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nr, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
                g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(ng, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
                b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nb, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
            } else
            {
                r = weighTwoValues(nr, r, fintensity);
                g = weighTwoValues(ng, g, fintensity);
                b = weighTwoValues(nb, b, fintensity);
            }
        }
    }

    void gamma(int level, int bright, int altMode, int noClamping) {
        if (noClamping == 0)
        {
            r = LUTgamma[clamp(r, 0, 65535)];
            g = LUTgamma[clamp(g, 0, 65535)];
            b = LUTgamma[clamp(b, 0, 65535)];
        } else
        {
            const float minu = min((float)r, min((float)g, (float)b));
            float maxu = max((float)r, max((float)g, (float)b));
            float nr = r;
            float ng = g;
            float nb = b;
            float offset = 0.0f;
            if (minu < 0.0f)
            {
                nr = r - minu;
                ng = g - minu;
                nb = b - minu;
                offset = minu;
            }
            float denominator = (maxu < 65535.0f ? 65535.0f : maxu) - offset;
            if (denominator <= 0.0f) denominator = 1.0f;

            nr = nr / denominator;
            ng = ng / denominator;
            nb = nb / denominator;

            nr = clamp(nr, 0.0f, 1.0f);
            ng = clamp(ng, 0.0f, 1.0f);
            nb = clamp(nb, 0.0f, 1.0f);

            const int thisLevel = (bright < 0 && altMode == 0 && level > 300) ? level + abs(bright) / 300 : level;
            const double gamma_val = 300.0 / (double)thisLevel;
            r = (int)round(denominator * pow((double)nr, gamma_val) + offset);
            g = (int)round(denominator * pow((double)ng, gamma_val) + offset);
            b = (int)round(denominator * pow((double)nb, gamma_val) + offset);

            if (bright < 0 && altMode == 0)
            {
                if (r < -165535) r = -165535;
                if (g < -165535) g = -165535;
                if (b < -165535) b = -165535;
            }
        }
    }

    void contrast(int level, int altContra, int linearGamma, float fintensity, int noClamping, float fip) {
        if (altContra == 1)
        {
            a = LUTcontra[clamp(a, 0, 65535)];
            return;
        }

        if (noClamping == 1)
        {
            const float minu = min((float)r, min((float)g, (float)b));
            float maxu = max((float)r, max((float)g, (float)b));
            float nr = r;
            float ng = g;
            float nb = b;
            float offset = 0.0f;
            const float thisMin = (level > 0) ? minu : abs(minu);
            float fi;
            if (level >= 0)
            {
                int gray = getGrayscaleAdvanced();
                nr = weighTwoValues(gray, r, fintensity);
                ng = weighTwoValues(gray, g, fintensity);
                nb = weighTwoValues(gray, b, fintensity);
                if (level < 19500)
                {
                    fi = level / 19500.0f;
                    r = weighTwoValues(nr, r, fi);
                    g = weighTwoValues(ng, g, fi);
                    b = weighTwoValues(nb, b, fi);
                } else
                {
                    r = nr;
                    g = ng;
                    b = nb;
                }
            }

            if (minu < 0.0f)
            {
                nr = r - minu;
                ng = g - minu;
                nb = b - minu;
                offset = minu * 2.0f + (float)level;
            }

            if (maxu < 65535.0f)
                maxu = 65535.0f;

            float mid = maxu / 2.0f;
            if (level > 0)
            {
                nr = floor(fip * (nr - mid)) + mid - offset;
                ng = floor(fip * (ng - mid)) + mid - offset;
                nb = floor(fip * (nb - mid)) + mid - offset;
                if (level < 16000)
                {
                    r = weighTwoValues(nr, r, fip);
                    g = weighTwoValues(ng, g, fip);
                    b = weighTwoValues(nb, b, fip);
                } else
                {
                    r = (int)nr;
                    g = (int)ng;
                    b = (int)nb;
                }
            } else
            {
                r = weighTwoValues(r, 32768, fip);
                g = weighTwoValues(g, 32768, fip);
                b = weighTwoValues(b, 32768, fip);
            }
        } else
        {
            if (level > 0)
            {
                int gray = getInt16grayscale(r, g, b);
                if (linearGamma == 1)
                {
                    r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
                    g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
                    b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
                } else
                {
                    r = weighTwoValues(gray, r, fintensity);
                    g = weighTwoValues(gray, g, fintensity);
                    b = weighTwoValues(gray, b, fintensity);
                }
            }

            r = LUTcontra[clamp(r, 0, 65535)];
            g = LUTcontra[clamp(g, 0, 65535)];
            b = LUTcontra[clamp(b, 0, 65535)];
        }
    }

    void saturation(int level, int altMode, int linearGamma, float saturation) {
        if (altMode > 1)
        {
            int gray = (altMode == 2) ? r : g;
            if (altMode == 3)
                gray = b;
            r = gray;
            g = gray;
            b = gray;
        } else if (altMode == 1)
        {
            HSLColor HSLu = ConvertRGBtoHSL();
            saturation = (level < 0) ? 0.001f : saturation;
            HSLColor newHSL = {HSLu.h, saturation, HSLu.l};
            RGBColorI newRGB = newHSL.ConvertHSLtoRGBint16();
            float fi = 0.0f;
            if (inRange(0, 16384, level))
                fi = level / 16384.0f;
            else if (inRange(-65535, 0, level))
                fi = abs(level) / 65535.0f;

            if (inRange(-65535, 16384, level))
            {
                r = weighTwoValues(newRGB.r, r, fi);
                g = weighTwoValues(newRGB.g, g, fi);
                b = weighTwoValues(newRGB.b, b, fi);
            } else
            {
                r = newRGB.r;
                g = newRGB.g;
                b = newRGB.b;
            }
        } else if (level < 0)
        {
            const int gray = getInt16grayscale(r, g, b);
            const float fintensity = clamp(abs(level), 0, 65535) / 65535.0f;
            if (linearGamma == 1)
            {
                r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
                g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
                b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(gray, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
            } else
            {
                r = weighTwoValues(gray, r, fintensity);
                g = weighTwoValues(gray, g, fintensity);
                b = weighTwoValues(gray, b, fintensity);
            }
        } else
        {
            const float max_val = max((float)r, max((float)g, (float)b));
            const float min_val = min((float)r, min((float)g, (float)b));
            const float luxAvg = (max_val + min_val) / 2.0f;
            const float factor = (level + 21823) / 21823.0f;

            float lux = clamp(getInt16grayscale(r, g, b) / 3.0f, 0.0f, 65535.0f);
            lux = weighTwoValues(lux, 0.0f, clamp(level, 0, 65535) / 65535.0f);

            r = clamp((int)round(factor * ((float)r - luxAvg) + luxAvg + lux), 0, 65535);
            g = clamp((int)round(factor * ((float)g - luxAvg) + luxAvg + lux), 0, 65535);
            b = clamp((int)round(factor * ((float)b - luxAvg) + luxAvg + lux), 0, 65535);
        }
    }

    void hueRotate(int degrees, float saturation, int altMode, int level) {
        HSLColor HSLu = ConvertRGBtoHSL();
        float hue = HSLu.h + (float)degrees;
        while (hue > 360.0f) hue -= 360.0f;
        while (hue < 0.0f) hue += 360.0f;

        HSLColor newHSL = {hue, HSLu.s + 0.01, HSLu.l};
        RGBColorI newRGB = newHSL.ConvertHSLtoRGBint16();
        float fi = 0.0f;
        if (inRange(0, 15, degrees))
            fi = degrees / 15.0f;
        else if (inRange(-15, 0, degrees))
            fi = abs(degrees) / 15.0f;

        if (inRange(-15, 15, degrees))
        {
            r = weighTwoValues(newRGB.r, r, fi);
            g = weighTwoValues(newRGB.g, g, fi);
            b = weighTwoValues(newRGB.b, b, fi);
        } else
        {
            r = newRGB.r;
            g = newRGB.g;
            b = newRGB.b;
        }
    }

    void tinto(int degrees, int level, int linearGamma) {
        HSLColor HSLu = ConvertRGBtoHSL();
        HSLColor newHSL = {(double)degrees, 0.5, HSLu.l};
        RGBColorI newRGB = newHSL.ConvertHSLtoRGBint16();
        const float fintensity = clamp(level, 0, 65535) / 65535.0f;
        if (linearGamma == 1)
        {
            r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(newRGB.r, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
            g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(newRGB.g, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
            b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(newRGB.b, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
        } else
        {
            r = weighTwoValues(newRGB.r, r, fintensity);
            g = weighTwoValues(newRGB.g, g, fintensity);
            b = weighTwoValues(newRGB.b, b, fintensity);
        }
    }

    void tint(float hue, int level, int altMode, int linearGamma) {
        if (altMode == 1)
            return tinto((int)hue, level, linearGamma);

        int z = getInt16grayscale(r, g, b);
        const float gray = z / 65535.0f;
        float normalized_hue = hue;
        while (normalized_hue > 360.0f) normalized_hue -= 360.0f;
        while (normalized_hue < 0.0f) normalized_hue += 360.0f;
        const int hi = (int)(floor(normalized_hue / 60.0f)) % 6;
        const float f = normalized_hue / 60.0f - floor(normalized_hue / 60.0f);
        const float q = gray * (1.0f - f);
        const float t = gray * (1.0f - (1.0f - f));
        int nr = 0, ng = 0, nb = 0;
        switch (hi) {
            case 0:
                nr = z;
                ng = (int)round(t * 65535.0f);
                nb = 0;
                break;
            case 1:
                nr = (int)round(q * 65535.0f);
                ng = z;
                nb = 0;
                break;
            case 2:
                nr = 0;
                ng = z;
                nb = (int)round(t * 65535.0f);
                break;
            case 3:
                nr = 0;
                ng = (int)round(q * 65535.0f);
                nb = z;
                break;
            case 4:
                nr = (int)round(t * 65535.0f);
                ng = 0;
                nb = z;
                break;
            case 5:
                nr = z;
                ng = 0;
                nb = (int)round(q * 65535.0f);
                break;
        }

        z = z / 3;
        const float fintensity = clamp(level, 0, 65535) / 65535.0f;
        nr = clamp(nr + z, 0, 65535);
        ng = clamp(ng + z, 0, 65535);
        nb = clamp(nb + z, 0, 65535);
        if (linearGamma == 1)
        {
            r = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nr, 0, 65535)], gamma_to_linearInt16[clamp(r, 0, 65535)], fintensity), 0, 65535)];
            g = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(ng, 0, 65535)], gamma_to_linearInt16[clamp(g, 0, 65535)], fintensity), 0, 65535)];
            b = linear_to_gammaInt16[clamp(weighTwoValues(gamma_to_linearInt16[clamp(nb, 0, 65535)], gamma_to_linearInt16[clamp(b, 0, 65535)], fintensity), 0, 65535)];
        } else
        {
            r = weighTwoValues(nr, r, fintensity);
            g = weighTwoValues(ng, g, fintensity);
            b = weighTwoValues(nb, b, fintensity);
        }
    }
};
