// Bloom Forge — multi-scale glow.
//
// Pipeline (all in float, premultiplied):
//   input -> (optional sRGB->linear) -> soft-knee bright pass
//         -> kLevels gaussian blurs with radius doubling per level, each done on a mip level
//            of the bright pass so cost does not grow with radius
//         -> weighted sum -> exposure / tint -> composite over source -> back to output depth.

#include "BloomForge.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <new>
#include <thread>
#include <vector>

namespace
{

constexpr int kLevels = 8;           // glow scales, radius halves from BF_RADIUS down
constexpr float kSigmaPerRadius = 0.5f;
constexpr float kExtentPerSigma = 3.5f;
constexpr A_long kMaxExtent = 8000;

// ---------- small utilities ----------

template <typename F>
void ParallelFor(int n, F fn)
{
    unsigned threads = std::max(1u, std::min(std::thread::hardware_concurrency(), 16u));
    if (n < 32 || threads == 1)
    {
        for (int i = 0; i < n; i++)
            fn(i);
        return;
    }
    std::atomic<int> next(0);
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned t = 0; t < threads; t++)
    {
        pool.emplace_back([&] {
            for (int i = next.fetch_add(1); i < n; i = next.fetch_add(1))
                fn(i);
        });
    }
    for (auto& t : pool)
        t.join();
}

inline float Clamp(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float SrgbToLinear(float c)
{
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float LinearToSrgb(float c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// Interleaved float image, C channels per pixel.
struct Image
{
    int w = 0, h = 0, c = 0;
    std::vector<float> px;

    void Resize(int W, int H, int C)
    {
        w = W;
        h = H;
        c = C;
        px.assign(static_cast<size_t>(W) * H * C, 0.0f);
    }
    float* At(int x, int y) { return px.data() + (static_cast<size_t>(y) * w + x) * c; }
    const float* At(int x, int y) const { return px.data() + (static_cast<size_t>(y) * w + x) * c; }
};

// ---------- gaussian blur via three box passes ----------

// Box widths whose triple convolution approximates a gaussian of the given sigma.
void BoxesForGauss(float sigma, int radii[3])
{
    const int n = 3;
    float wIdeal = std::sqrt(12.0f * sigma * sigma / n + 1.0f);
    int wl = static_cast<int>(std::floor(wIdeal));
    if (wl % 2 == 0)
        wl--;
    int wu = wl + 2;
    float mIdeal = (12.0f * sigma * sigma - n * wl * wl - 4.0f * n * wl - 3.0f * n) / (-4.0f * wl - 4.0f);
    int m = static_cast<int>(std::lround(mIdeal));
    for (int i = 0; i < n; i++)
        radii[i] = ((i < m ? wl : wu) - 1) / 2;
}

// One box pass over a line of n pixels; samples outside the line count as transparent.
void BoxLine(const float* in, float* out, int n, int c, int r)
{
    double sum[4] = {0, 0, 0, 0};
    const double inv = 1.0 / (2 * r + 1);
    for (int k = 0; k <= r && k < n; k++)
        for (int ch = 0; ch < c; ch++)
            sum[ch] += in[k * c + ch];

    for (int x = 0; x < n; x++)
    {
        for (int ch = 0; ch < c; ch++)
            out[x * c + ch] = static_cast<float>(sum[ch] * inv);
        int add = x + r + 1, sub = x - r;
        if (add < n)
            for (int ch = 0; ch < c; ch++)
                sum[ch] += in[add * c + ch];
        if (sub >= 0)
            for (int ch = 0; ch < c; ch++)
                sum[ch] -= in[sub * c + ch];
    }
}

void BlurAxis(Image& img, float sigma, bool horizontal)
{
    if (sigma < 0.3f)
        return;
    int radii[3];
    BoxesForGauss(sigma, radii);

    const int lines = horizontal ? img.h : img.w;
    const int n = horizontal ? img.w : img.h;
    const int c = img.c;

    ParallelFor(lines, [&](int line) {
        std::vector<float> a(static_cast<size_t>(n) * c), b(static_cast<size_t>(n) * c);
        for (int i = 0; i < n; i++)
        {
            const float* p = horizontal ? img.At(i, line) : img.At(line, i);
            std::copy(p, p + c, &a[static_cast<size_t>(i) * c]);
        }
        for (int pass = 0; pass < 3; pass++)
        {
            if (radii[pass] > 0)
            {
                BoxLine(a.data(), b.data(), n, c, radii[pass]);
                a.swap(b);
            }
        }
        for (int i = 0; i < n; i++)
        {
            float* p = horizontal ? img.At(i, line) : img.At(line, i);
            std::copy(&a[static_cast<size_t>(i) * c], &a[static_cast<size_t>(i) * c] + c, p);
        }
    });
}

Image Downsample2x(const Image& src)
{
    Image dst;
    dst.Resize((src.w + 1) / 2, (src.h + 1) / 2, src.c);
    ParallelFor(dst.h, [&](int y) {
        for (int x = 0; x < dst.w; x++)
        {
            float* d = dst.At(x, y);
            for (int dy = 0; dy < 2; dy++)
            {
                int sy = y * 2 + dy;
                if (sy >= src.h)
                    continue;
                for (int dx = 0; dx < 2; dx++)
                {
                    int sx = x * 2 + dx;
                    if (sx >= src.w)
                        continue;
                    const float* s = src.At(sx, sy);
                    for (int ch = 0; ch < src.c; ch++)
                        d[ch] += s[ch] * 0.25f;
                }
            }
        }
    });
    return dst;
}

// acc += weight * bilinear_upsample(level), level being `scale` times smaller than acc.
void AccumulateUpsampled(Image& acc, const Image& level, int scale, float weight)
{
    const float inv = 1.0f / scale;
    ParallelFor(acc.h, [&](int y) {
        float fy = (y + 0.5f) * inv - 0.5f;
        int y0 = static_cast<int>(std::floor(fy));
        float ty = fy - y0;
        for (int x = 0; x < acc.w; x++)
        {
            float fx = (x + 0.5f) * inv - 0.5f;
            int x0 = static_cast<int>(std::floor(fx));
            float tx = fx - x0;
            float* d = acc.At(x, y);
            for (int j = 0; j < 2; j++)
            {
                int sy = y0 + j;
                if (sy < 0 || sy >= level.h)
                    continue;
                float wy = j ? ty : 1.0f - ty;
                for (int i = 0; i < 2; i++)
                {
                    int sx = x0 + i;
                    if (sx < 0 || sx >= level.w)
                        continue;
                    float wgt = weight * wy * (i ? tx : 1.0f - tx);
                    const float* s = level.At(sx, sy);
                    for (int ch = 0; ch < acc.c; ch++)
                        d[ch] += s[ch] * wgt;
                }
            }
        }
    });
}

// ---------- parameters ----------

struct GlowParams
{
    float threshold;
    float softness;
    float radius;   // in layer pixels at full resolution
    float exposure; // stops
    float falloff;  // 0..1
    float aspect;   // stops, >0 stretches horizontally
    float tint[3];
    float tintAmount; // 0..1
    int composite;
    bool linear;
    float downX, downY; // render downsample factors
};

// Gaussian sigma of the largest level along each axis, in buffer pixels.
void LargestSigma(float radius, float aspect, float downX, float downY, float* sx, float* sy)
{
    float sigma = radius * kSigmaPerRadius;
    *sx = sigma * std::pow(2.0f, aspect) * downX;
    *sy = sigma * std::pow(2.0f, -aspect) * downY;
}

// ---------- pixel depth handling ----------

template <typename P>
struct Depth;
template <>
struct Depth<PF_Pixel8>
{
    static constexpr float kMax = PF_MAX_CHAN8;
    static constexpr bool kFloat = false;
};
template <>
struct Depth<PF_Pixel16>
{
    static constexpr float kMax = PF_MAX_CHAN16;
    static constexpr bool kFloat = false;
};
template <>
struct Depth<PF_PixelFloat>
{
    static constexpr float kMax = 1.0f;
    static constexpr bool kFloat = true;
};

template <typename P>
inline P* PixelAt(PF_EffectWorld* world, int x, int y)
{
    return reinterpret_cast<P*>(reinterpret_cast<char*>(world->data) + static_cast<size_t>(y) * world->rowbytes) + x;
}

template <typename T>
inline T ToChannel(float v, float maxV)
{
    return static_cast<T>(Clamp(v, 0.0f, 1.0f) * maxV + 0.5f);
}

template <typename P>
void Process(const GlowParams& gp, PF_EffectWorld* inW, PF_EffectWorld* outW, const BloomRects& rects)
{
    const float maxV = Depth<P>::kMax;
    const bool isFloat = Depth<P>::kFloat;

    // Working area: union of input and output rects, in layer space.
    const A_long L = std::min(rects.in_rect.left, rects.out_rect.left);
    const A_long T = std::min(rects.in_rect.top, rects.out_rect.top);
    const A_long R = std::max(rects.in_rect.right, rects.out_rect.right);
    const A_long B = std::max(rects.in_rect.bottom, rects.out_rect.bottom);
    const int W = static_cast<int>(R - L), H = static_cast<int>(B - T);
    if (W <= 0 || H <= 0)
        return;

    const int inOffX = static_cast<int>(rects.in_rect.left - L), inOffY = static_cast<int>(rects.in_rect.top - T);
    const int inW_ = std::min<int>(inW->width, static_cast<int>(rects.in_rect.right - rects.in_rect.left));
    const int inH_ = std::min<int>(inW->height, static_cast<int>(rects.in_rect.bottom - rects.in_rect.top));

    // Decode table for integer depths.
    std::vector<float> decode;
    if (!isFloat && gp.linear)
    {
        decode.resize(static_cast<size_t>(maxV) + 1);
        for (size_t i = 0; i < decode.size(); i++)
            decode[i] = SrgbToLinear(i / maxV);
    }

    // 1. Source in premultiplied float (linear if requested).
    Image src;
    src.Resize(W, H, 4);
    ParallelFor(inH_, [&](int y) {
        const P* p = PixelAt<P>(inW, 0, y);
        for (int x = 0; x < inW_; x++, p++)
        {
            float* s = src.At(x + inOffX, y + inOffY);
            float a = p->alpha / maxV;
            float rgb[3] = {p->red / maxV, p->green / maxV, p->blue / maxV};
            if (gp.linear && a > 0.0f)
            {
                if (isFloat)
                {
                    for (int ch = 0; ch < 3; ch++)
                        rgb[ch] = SrgbToLinear(rgb[ch] / a) * a;
                }
                else
                {
                    // Integer pixels: unpremultiply, look up, premultiply.
                    for (int ch = 0; ch < 3; ch++)
                    {
                        float straight = Clamp(rgb[ch] / a, 0.0f, 1.0f);
                        rgb[ch] = decode[static_cast<size_t>(straight * maxV + 0.5f)] * a;
                    }
                }
            }
            s[0] = rgb[0];
            s[1] = rgb[1];
            s[2] = rgb[2];
            s[3] = a;
        }
    });

    // 2. Soft-knee bright pass (RGB only — glow alpha is derived from its brightness later).
    std::vector<Image> mips(1);
    mips[0].Resize(W, H, 3);
    {
        const float t = gp.threshold;
        const float knee = std::max(t * gp.softness, 1e-5f);
        ParallelFor(H, [&](int y) {
            for (int x = 0; x < W; x++)
            {
                const float* s = src.At(x, y);
                float b = std::max(s[0], std::max(s[1], s[2]));
                float soft = Clamp(b - t + knee, 0.0f, 2.0f * knee);
                soft = soft * soft / (4.0f * knee);
                float contrib = std::max(soft, b - t) / std::max(b, 1e-5f);
                float* d = mips[0].At(x, y);
                for (int ch = 0; ch < 3; ch++)
                    d[ch] = std::max(s[ch], 0.0f) * contrib;
            }
        });
    }

    // 3. Multi-scale blur, each level on the coarsest mip that still resolves it.
    float sigmaX, sigmaY;
    LargestSigma(gp.radius, gp.aspect, gp.downX, gp.downY, &sigmaX, &sigmaY);

    // Weights are not normalized: a wide blur of a thin source is already faint, so every level
    // keeps its full strength and Falloff only dims the wider levels relative to the narrow ones.
    float weights[kLevels];
    const float q = 1.0f - 0.5f * Clamp(gp.falloff, 0.0f, 1.0f);
    for (int i = 0; i < kLevels; i++)
        weights[i] = std::pow(q, static_cast<float>(i));

    Image acc;
    acc.Resize(W, H, 3);
    for (int i = 0; i < kLevels; i++)
    {
        float scaleDown = std::pow(2.0f, static_cast<float>(kLevels - 1 - i));
        float sx = sigmaX / scaleDown, sy = sigmaY / scaleDown;

        // Go one mip down while the smaller sigma would still be >= 2 px there.
        int m = 0;
        const float minSigma = std::min(sx, sy);
        while (m < 12 && minSigma / (1 << (m + 1)) >= 2.0f)
        {
            if (static_cast<int>(mips.size()) <= m + 1)
            {
                if (mips[m].w < 8 || mips[m].h < 8)
                    break;
                Image next = Downsample2x(mips[m]);
                mips.push_back(std::move(next));
            }
            m++;
        }

        Image level = mips[m];
        BlurAxis(level, sx / (1 << m), true);
        BlurAxis(level, sy / (1 << m), false);
        AccumulateUpsampled(acc, level, 1 << m, weights[i]);
    }

    // 4. Exposure, tint, composite, back to output depth.
    float tint[3];
    for (int ch = 0; ch < 3; ch++)
        tint[ch] = gp.linear ? SrgbToLinear(gp.tint[ch]) : gp.tint[ch];
    const float gain = std::pow(2.0f, gp.exposure);
    const int outOffX = static_cast<int>(rects.out_rect.left - L), outOffY = static_cast<int>(rects.out_rect.top - T);
    const int outWidth = std::min<int>(outW->width, static_cast<int>(rects.out_rect.right - rects.out_rect.left));
    const int outHeight = std::min<int>(outW->height, static_cast<int>(rects.out_rect.bottom - rects.out_rect.top));

    ParallelFor(outHeight, [&](int y) {
        P* o = PixelAt<P>(outW, 0, y);
        for (int x = 0; x < outWidth; x++, o++)
        {
            const float* s = src.At(x + outOffX, y + outOffY);
            const float* a = acc.At(x + outOffX, y + outOffY);

            float g[3] = {a[0] * gain, a[1] * gain, a[2] * gain};
            if (gp.tintAmount > 0.0f)
            {
                float lum = 0.2126f * g[0] + 0.7152f * g[1] + 0.0722f * g[2];
                for (int ch = 0; ch < 3; ch++)
                    g[ch] += (lum * tint[ch] - g[ch]) * gp.tintAmount;
            }
            // Blur happens in linear light, but AE composites the layer in the project's encoded
            // space. The glow is emitted light, so it is encoded on its own (not divided by alpha):
            // a linear 0.05 then reads as ~0.25 over whatever lies underneath, as it should.
            float base[3];
            for (int ch = 0; ch < 3; ch++)
            {
                if (gp.linear)
                {
                    base[ch] = s[3] > 0.0f ? LinearToSrgb(s[ch] / s[3]) * s[3] : 0.0f;
                    g[ch] = LinearToSrgb(std::max(g[ch], 0.0f));
                }
                else
                {
                    base[ch] = s[ch];
                }
            }
            float glowA = Clamp(std::max(g[0], std::max(g[1], g[2])), 0.0f, 1.0f);

            float rgb[3], alpha;
            switch (gp.composite)
            {
            case Composite_SCREEN:
                for (int ch = 0; ch < 3; ch++)
                    rgb[ch] = base[ch] + g[ch] * (1.0f - Clamp(base[ch], 0.0f, 1.0f));
                alpha = s[3] + glowA * (1.0f - s[3]);
                break;
            case Composite_GLOW_ONLY:
                for (int ch = 0; ch < 3; ch++)
                    rgb[ch] = g[ch];
                alpha = glowA;
                break;
            default: // Composite_ADD
                for (int ch = 0; ch < 3; ch++)
                    rgb[ch] = base[ch] + g[ch];
                alpha = s[3] + glowA * (1.0f - s[3]);
                break;
            }
            alpha = Clamp(alpha, 0.0f, 1.0f);

            // Integer buffers are premultiplied and cannot hold color brighter than alpha.
            if (!isFloat)
                for (int ch = 0; ch < 3; ch++)
                    rgb[ch] = Clamp(rgb[ch], 0.0f, alpha);

            if constexpr (Depth<P>::kFloat)
            {
                o->red = rgb[0];
                o->green = rgb[1];
                o->blue = rgb[2];
                o->alpha = alpha;
            }
            else
            {
                using Ch = decltype(o->red);
                o->red = ToChannel<Ch>(rgb[0], maxV);
                o->green = ToChannel<Ch>(rgb[1], maxV);
                o->blue = ToChannel<Ch>(rgb[2], maxV);
                o->alpha = ToChannel<Ch>(alpha, maxV);
            }
        }
    });
}

PF_Err Checkout(PF_InData* in_data, A_long index, PF_ParamDef* def)
{
    AEFX_CLR_STRUCT(*def);
    return PF_CHECKOUT_PARAM(in_data, index, in_data->current_time, in_data->time_step, in_data->time_scale, def);
}

float Ratio(const PF_RationalScale& s)
{
    return s.den ? static_cast<float>(s.num) / static_cast<float>(s.den) : 1.0f;
}

void DeleteRects(void* data)
{
    delete reinterpret_cast<BloomRects*>(data);
}

} // namespace

// ---------- command handlers ----------

static PF_Err About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    PF_SPRINTF(out_data->return_msg,
               "%s v%d.%d\rMulti-scale glow with soft threshold, linear blending and 32 bpc support.",
               BF_NAME,
               MAJOR_VERSION,
               MINOR_VERSION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_I_EXPAND_BUFFER;
    out_data->out_flags2 =
        PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    PF_ADD_FLOAT_SLIDERX("Threshold", 0, 10, 0, 1, 0.25, PF_Precision_HUNDREDTHS, 0, 0, THRESHOLD_DISK_ID);
    PF_ADD_FLOAT_SLIDERX("Threshold Softness", 0, 1, 0, 1, 0.5, PF_Precision_HUNDREDTHS, 0, 0, SOFTNESS_DISK_ID);
    PF_ADD_FLOAT_SLIDERX("Radius", 1, 4000, 1, 1000, 150, PF_Precision_TENTHS, 0, 0, RADIUS_DISK_ID);
    PF_ADD_FLOAT_SLIDERX("Exposure", -10, 10, -4, 4, 0, PF_Precision_HUNDREDTHS, 0, 0, EXPOSURE_DISK_ID);
    PF_ADD_FLOAT_SLIDERX(
        "Falloff", 0, 100, 0, 100, 25, PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT, 0, FALLOFF_DISK_ID);
    PF_ADD_FLOAT_SLIDERX("Aspect", -3, 3, -1, 1, 0, PF_Precision_HUNDREDTHS, 0, 0, ASPECT_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Tint Color", 255, 170, 90, TINT_COLOR_DISK_ID);
    PF_ADD_FLOAT_SLIDERX(
        "Tint Amount", 0, 100, 0, 100, 0, PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT, 0, TINT_AMOUNT_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Composite", Composite_NUM - 1, Composite_ADD, "Add|Screen|Glow Only", COMPOSITE_DISK_ID);
    PF_ADD_CHECKBOXX("Linear Blending", TRUE, 0, LINEAR_DISK_ID);

    out_data->num_params = BF_NUM_PARAMS;
    return err;
}

static PF_Err PreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;
    PF_ParamDef radius_param, aspect_param;

    // Params checked out during pre-render are checked in by AE automatically.
    ERR(Checkout(in_data, BF_RADIUS, &radius_param));
    ERR(Checkout(in_data, BF_ASPECT, &aspect_param));
    if (err)
        return err;

    float sx, sy;
    LargestSigma(static_cast<float>(radius_param.u.fs_d.value),
                 static_cast<float>(aspect_param.u.fs_d.value),
                 Ratio(in_data->downsample_x),
                 Ratio(in_data->downsample_y),
                 &sx,
                 &sy);
    const A_long ex = std::min<A_long>(static_cast<A_long>(std::ceil(sx * kExtentPerSigma)) + 4, kMaxExtent);
    const A_long ey = std::min<A_long>(static_cast<A_long>(std::ceil(sy * kExtentPerSigma)) + 4, kMaxExtent);

    // Pixels up to the glow extent outside the requested area still contribute to it.
    req.rect.left -= ex;
    req.rect.top -= ey;
    req.rect.right += ex;
    req.rect.bottom += ey;
    req.preserve_rgb_of_zero_alpha = FALSE;

    ERR(extra->cb->checkout_layer(in_data->effect_ref,
                                  BF_INPUT,
                                  BF_INPUT,
                                  &req,
                                  in_data->current_time,
                                  in_data->time_step,
                                  in_data->time_scale,
                                  &in_result));
    if (err)
        return err;

    PF_LRect result = in_result.result_rect;
    if (result.right > result.left && result.bottom > result.top)
    {
        result.left -= ex;
        result.top -= ey;
        result.right += ex;
        result.bottom += ey;
    }
    const PF_LRect& asked = extra->input->output_request.rect;
    result.left = std::max(result.left, asked.left);
    result.top = std::max(result.top, asked.top);
    result.right = std::min(result.right, asked.right);
    result.bottom = std::min(result.bottom, asked.bottom);
    if (result.right < result.left)
        result.right = result.left;
    if (result.bottom < result.top)
        result.bottom = result.top;

    PF_LRect max_result = in_result.max_result_rect;
    max_result.left -= ex;
    max_result.top -= ey;
    max_result.right += ex;
    max_result.bottom += ey;

    extra->output->result_rect = result;
    extra->output->max_result_rect = max_result;

    BloomRects* rects = new BloomRects;
    rects->in_rect = in_result.result_rect;
    rects->out_rect = result;
    extra->output->pre_render_data = rects;
    extra->output->delete_pre_render_data_func = DeleteRects;

    return err;
}

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
    PF_EffectWorld *inputP = NULL, *outputP = NULL;
    const BloomRects* rects = reinterpret_cast<const BloomRects*>(extra->input->pre_render_data);

    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, BF_INPUT, &inputP));
    ERR(extra->cb->checkout_output(in_data->effect_ref, &outputP));
    if (err || !inputP || !outputP || !rects)
        return err;

    PF_ParamDef p[BF_NUM_PARAMS];
    for (A_long i = 1; i < BF_NUM_PARAMS; i++)
        ERR(Checkout(in_data, i, &p[i]));

    GlowParams gp;
    gp.threshold = static_cast<float>(p[BF_THRESHOLD].u.fs_d.value);
    gp.softness = static_cast<float>(p[BF_SOFTNESS].u.fs_d.value);
    gp.radius = static_cast<float>(p[BF_RADIUS].u.fs_d.value);
    gp.exposure = static_cast<float>(p[BF_EXPOSURE].u.fs_d.value);
    gp.falloff = static_cast<float>(p[BF_FALLOFF].u.fs_d.value / 100.0);
    gp.aspect = static_cast<float>(p[BF_ASPECT].u.fs_d.value);
    gp.tint[0] = p[BF_TINT_COLOR].u.cd.value.red / 255.0f;
    gp.tint[1] = p[BF_TINT_COLOR].u.cd.value.green / 255.0f;
    gp.tint[2] = p[BF_TINT_COLOR].u.cd.value.blue / 255.0f;
    gp.tintAmount = static_cast<float>(p[BF_TINT_AMOUNT].u.fs_d.value / 100.0);
    gp.composite = p[BF_COMPOSITE].u.pd.value;
    gp.linear = p[BF_LINEAR].u.bd.value != 0;
    gp.downX = Ratio(in_data->downsample_x);
    gp.downY = Ratio(in_data->downsample_y);

    PF_PixelFormat format = PF_PixelFormat_INVALID;
    {
        AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
        ERR(worldSuite->PF_GetPixelFormat(inputP, &format));
    }

    if (!err)
    {
        try
        {
            switch (format)
            {
            case PF_PixelFormat_ARGB128:
                Process<PF_PixelFloat>(gp, inputP, outputP, *rects);
                break;
            case PF_PixelFormat_ARGB64:
                Process<PF_Pixel16>(gp, inputP, outputP, *rects);
                break;
            case PF_PixelFormat_ARGB32:
                Process<PF_Pixel8>(gp, inputP, outputP, *rects);
                break;
            default:
                err = PF_Err_BAD_CALLBACK_PARAM;
                break;
            }
        }
        catch (const std::bad_alloc&)
        {
            err = PF_Err_OUT_OF_MEMORY;
        }
    }

    for (A_long i = 1; i < BF_NUM_PARAMS; i++)
        ERR2(PF_CHECKIN_PARAM(in_data, &p[i]));

    return err;
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,
                                                     PF_PluginDataCB2 inPluginDataCallBackPtr,
                                                     SPBasicSuite* inSPBasicSuitePtr,
                                                     const char* inHostName,
                                                     const char* inHostVersion)
{
    // The registration macro assigns to a local named `result`.
    PF_Err result = PF_Err_INVALID_CALLBACK;
    result = PF_REGISTER_EFFECT_EXT2(inPtr,
                                     inPluginDataCallBackPtr,
                                     BF_NAME,
                                     BF_MATCH_NAME,
                                     BF_CATEGORY,
                                     AE_RESERVED_INFO,
                                     "EffectMain",
                                     BF_SUPPORT_URL);
    return result;
}

PF_Err EffectMain(
    PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra)
{
    PF_Err err = PF_Err_NONE;
    try
    {
        switch (cmd)
        {
        case PF_Cmd_ABOUT:
            err = About(in_data, out_data, params, output);
            break;
        case PF_Cmd_GLOBAL_SETUP:
            err = GlobalSetup(in_data, out_data, params, output);
            break;
        case PF_Cmd_PARAMS_SETUP:
            err = ParamsSetup(in_data, out_data, params, output);
            break;
        case PF_Cmd_SMART_PRE_RENDER:
            err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
            break;
        case PF_Cmd_SMART_RENDER:
            err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
            break;
        }
    }
    catch (PF_Err& thrown_err)
    {
        err = thrown_err;
    }
    return err;
}
