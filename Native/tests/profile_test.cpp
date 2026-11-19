// Prints the glow falloff next to a white 20 px stroke on a transparent 8-bit layer.
// Build: tests\run_profile_test.ps1

#include "../BloomForge.cpp"

#include <cstdio>

int main()
{
    const int W = 1200, H = 1024, strokeL = 590, strokeR = 610;
    std::vector<PF_Pixel8> pixels(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; y++)
        for (int x = strokeL; x < strokeR; x++)
            pixels[static_cast<size_t>(y) * W + x] = PF_Pixel8{255, 255, 255, 255};
    std::vector<PF_Pixel8> out(pixels.size());

    PF_EffectWorld inW = {}, outW = {};
    inW.width = outW.width = W;
    inW.height = outW.height = H;
    inW.rowbytes = outW.rowbytes = W * sizeof(PF_Pixel8);
    inW.data = reinterpret_cast<PF_PixelPtr>(pixels.data());
    outW.data = reinterpret_cast<PF_PixelPtr>(out.data());

    BloomRects rects;
    rects.in_rect = {0, 0, W, H};
    rects.out_rect = rects.in_rect;

    GlowParams gp = {};
    gp.threshold = 0.25f;
    gp.softness = 0.5f;
    gp.radius = 150.0f;
    gp.exposure = 0.0f;
    gp.falloff = 0.25f;
    gp.tint[0] = gp.tint[1] = gp.tint[2] = 1.0f;
    gp.composite = Composite_ADD;
    gp.linear = true;
    gp.downX = gp.downY = 1.0f;

    Process<PF_Pixel8>(gp, &inW, &outW, rects);

    std::printf("distance from stroke edge -> glow alpha (0..255)\n");
    const int distances[] = {1, 5, 10, 20, 40, 60, 100, 150, 250, 400};
    for (int d : distances)
    {
        const PF_Pixel8& p = out[static_cast<size_t>(H / 2) * W + strokeR - 1 + d];
        std::printf("%4d px: %3d\n", d, p.alpha);
    }
    return 0;
}
