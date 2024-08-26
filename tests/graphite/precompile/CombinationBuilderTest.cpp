/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tests/Test.h"

#if defined(SK_GRAPHITE)

#include "include/core/SkColorSpace.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/gpu/graphite/precompile/PrecompileBlender.h"
#include "include/gpu/graphite/precompile/PrecompileColorFilter.h"
#include "include/gpu/graphite/precompile/PrecompileRuntimeEffect.h"
#include "include/gpu/graphite/precompile/PrecompileShader.h"
#include "src/gpu/graphite/ContextPriv.h"
#include "src/gpu/graphite/KeyContext.h"
#include "src/gpu/graphite/PipelineData.h"
#include "src/gpu/graphite/PrecompileInternal.h"
#include "src/gpu/graphite/Renderer.h"
#include "src/gpu/graphite/RuntimeEffectDictionary.h"
#include "src/gpu/graphite/precompile/PaintOptionsPriv.h"

#include <array>

using namespace::skgpu::graphite;

namespace {

// colorfilters
static constexpr int kExpectedBlendCFCombos = 15;
static constexpr int kExpectedColorSpaceCFCombos = 1;
static constexpr int kExpectedHighContrastCFCombos = 1;
static constexpr int kExpectedLightingCFCombos = 1;
static constexpr int kExpectedLumaCFCombos = 1;
static constexpr int kExpectedMatrixCFCombos = 1;
static constexpr int kExpectedOverdrawCFCombos = 1;
static constexpr int kExpectedTableCFCombos = 1;

// shaders
static constexpr int kExpectedGradientCombos = 3;
static constexpr int kExpectedImageCombos = 6;
static constexpr int kExpectedPerlinNoiseCombos = 1;
static constexpr int kExpectedPictureCombos = 12;
static constexpr int kExpectedRawImageCombos = 3;
static constexpr int kExpectedSolidColorCombos = 1;


// A default kSrcOver blend mode will be supplied if no other blend options are added
void no_blend_mode_option_test(const KeyContext& keyContext,
                               PipelineDataGatherer* gatherer,
                               skiatest::Reporter* reporter) {
    PaintOptions paintOptions;
    paintOptions.setShaders({ PrecompileShaders::Color() });

    REPORTER_ASSERT(reporter, paintOptions.priv().numCombinations() == 1);

    std::vector<UniquePaintParamsID> precompileIDs;
    paintOptions.priv().buildCombinations(keyContext,
                                          gatherer,
                                          DrawTypeFlags::kNone,
                                          /* withPrimitiveBlender= */ false,
                                          Coverage::kNone,
                                          [&precompileIDs](UniquePaintParamsID id,
                                                           DrawTypeFlags,
                                                           bool /* withPrimitiveBlender */,
                                                           Coverage) {
                                                               precompileIDs.push_back(id);
                                                           });

    SkASSERT(precompileIDs.size() == 1);
}

// This test checks that the 'PaintOptions::numCombinations' method and the number actually
// generated by 'buildCombinations' agree with the expected number of combinations.
void run_test(const KeyContext& keyContext,
              PipelineDataGatherer* gatherer,
              skiatest::Reporter* reporter,
              const PaintOptions& paintOptions,
              int expectedNumOptions) {
    REPORTER_ASSERT(reporter, paintOptions.priv().numCombinations() == expectedNumOptions,
                    "expected %d, but was %d",
                    expectedNumOptions, paintOptions.priv().numCombinations());

    std::vector<UniquePaintParamsID> precompileIDs;
    paintOptions.priv().buildCombinations(keyContext,
                                          gatherer,
                                          DrawTypeFlags::kNone,
                                          /* withPrimitiveBlender= */ false,
                                          Coverage::kNone,
                                          [&precompileIDs](UniquePaintParamsID id,
                                                           DrawTypeFlags,
                                                           bool /* withPrimitiveBlender */,
                                                           Coverage) {
                                              precompileIDs.push_back(id);
                                          });

    SkASSERT(static_cast<int>(precompileIDs.size()) == expectedNumOptions);
}

void big_test(const KeyContext& keyContext,
              PipelineDataGatherer* gatherer,
              skiatest::Reporter* reporter) {

    static constexpr int kNumExpected = 444;
    // paintOptions (444 = 4*111)
    //  |- (111 = 3+108) sweepGrad_0 (3) |
    //  |                blendShader_0 (108 = 1*4*27)
    //  |                 |- 0: (1)       kSrc (1)
    //  |                 |- 1: (4=3+1)   (dsts) linearGrad_0 (3) | solid_0 (1)
    //  |                 |- 2: (27=3+24) (srcs) linearGrad_1 (3) |
    //  |                                        blendShader_1 (24=1*4*6)
    //  |                                         |- 0: (1) kDst (1)
    //  |                                         |- 1: (4=3+1) (dsts) radGrad_0 (3) | solid_1 (1)
    //  |                                         |- 2: (6) (srcs) imageShader_0 (6)
    //  |
    //  |- (4) 4-built-in-blend-modes

    PaintOptions paintOptions;

    // first, shaders. First top-level option (sweepGrad_0)
    sk_sp<PrecompileShader> sweepGrad_0 = PrecompileShaders::SweepGradient();

    std::array<SkBlendMode, 1> blendModes{ SkBlendMode::kSrc };

    std::vector<SkBlendMode> moreBlendModes{ SkBlendMode::kDst };

    // Second top-level option (blendShader_0)
    auto blendShader_0 = PrecompileShaders::Blend(
                                SkSpan<const SkBlendMode>(blendModes),          // std::array
                                {                                               // initializer_list
                                    PrecompileShaders::LinearGradient(),
                                    PrecompileShaders::Color()
                                },
                                {
                                    PrecompileShaders::LinearGradient(),
                                    PrecompileShaders::Blend(
                                            SkSpan<const SkBlendMode>(moreBlendModes),// std::vector
                                            {
                                                PrecompileShaders::RadialGradient(),
                                                PrecompileShaders::Color()
                                            },
                                            {
                                                PrecompileShaders::Image()
                                            })
                                });

    paintOptions.setShaders({ sweepGrad_0, blendShader_0 });

    static const SkBlendMode kEvenMoreBlendModes[] = {
        SkBlendMode::kSrcOver,
        SkBlendMode::kSrc,
        SkBlendMode::kDstOver,
        SkBlendMode::kDst
    };

    // now, blend modes
    paintOptions.setBlendModes(kEvenMoreBlendModes);                             // c array

    REPORTER_ASSERT(reporter, paintOptions.priv().numCombinations() == kNumExpected,
                    "Actual # of combinations %d", paintOptions.priv().numCombinations());

    std::vector<UniquePaintParamsID> precompileIDs;
    paintOptions.priv().buildCombinations(keyContext,
                                          gatherer,
                                          DrawTypeFlags::kNone,
                                          /* withPrimitiveBlender= */ false,
                                          Coverage::kNone,
                                          [&precompileIDs](UniquePaintParamsID id,
                                                           DrawTypeFlags,
                                                           bool /* withPrimitiveBlender */,
                                                           Coverage) {
                                                               precompileIDs.push_back(id);
                                                           });

    SkASSERT(precompileIDs.size() == kNumExpected);
}

template <typename T>
std::vector<sk_sp<T>> create_runtime_combos(
        skiatest::Reporter* reporter,
        SkRuntimeEffect::Result effectFactory(SkString),
        sk_sp<T> precompileFactory(sk_sp<SkRuntimeEffect>,
                                   SkSpan<const PrecompileChildOptions> childOptions),
        const char* redCode,
        const char* greenCode,
        const char* blueCode,
        const char* combineCode) {
    auto [redEffect, error1] = effectFactory(SkString(redCode));
    REPORTER_ASSERT(reporter, redEffect, "%s", error1.c_str());
    auto [greenEffect, error2] = effectFactory(SkString(greenCode));
    REPORTER_ASSERT(reporter, greenEffect, "%s", error2.c_str());
    auto [blueEffect, error3] = effectFactory(SkString(blueCode));
    REPORTER_ASSERT(reporter, blueEffect, "%s", error3.c_str());
    auto [combineEffect, error4] = effectFactory(SkString(combineCode));
    REPORTER_ASSERT(reporter, combineEffect, "%s", error4.c_str());

    sk_sp<T> red = precompileFactory(redEffect, {});
    REPORTER_ASSERT(reporter, red);

    sk_sp<T> green = precompileFactory(greenEffect, {});
    REPORTER_ASSERT(reporter, green);

    sk_sp<T> blue = precompileFactory(blueEffect, {});
    REPORTER_ASSERT(reporter, blue);

    sk_sp<T> combine = precompileFactory(combineEffect, { { red, green },
                                                          { blue, sk_sp<T>(nullptr) } });
    REPORTER_ASSERT(reporter, combine);

    return { combine };
}

void runtime_effect_test(const KeyContext& keyContext,
                         PipelineDataGatherer* gatherer,
                         skiatest::Reporter* reporter) {
    // paintOptions (64 = 4*4*4)
    //  |- combineShader (4)
    //  |       0: redShader  | greenShader
    //  |       1: blueShader | nullptr
    //  |
    //  |- combineColorFilter (4)
    //  |       0: redColorFilter  | greenColorFilter
    //  |       1: blueColorFilter | nullptr
    //  |
    //  |- combineBlender (4)
    //  |       0: redBlender  | greenBlender
    //  |       1: blueBlender | nullptr

    PaintOptions paintOptions;

    // shaders
    {
        static const char* kRedS = R"(
            half4 main(vec2 fragcoord) { return half4(.5, 0, 0, .5); }
        )";
        static const char* kGreenS = R"(
            half4 main(vec2 fragcoord) { return half4(0, .5, 0, .5); }
        )";
        static const char* kBlueS = R"(
            half4 main(vec2 fragcoord) { return half4(0, 0, .5, .5); }
        )";

        static const char* kCombineS = R"(
            uniform shader first;
            uniform shader second;
            half4 main(vec2 fragcoords) {
                return first.eval(fragcoords) + second.eval(fragcoords);
            }
        )";

        std::vector<sk_sp<PrecompileShader>> combinations =
            create_runtime_combos<PrecompileShader>(reporter,
                                                    SkRuntimeEffect::MakeForShader,
                                                    PrecompileRuntimeEffects::MakePrecompileShader,
                                                    kRedS,
                                                    kGreenS,
                                                    kBlueS,
                                                    kCombineS);
        paintOptions.setShaders(combinations);
    }

    // color filters
    {
        static const char* kRedCF = R"(
            half4 main(half4 color) { return half4(.5, 0, 0, .5); }
        )";
        static const char* kGreenCF = R"(
            half4 main(half4 color) { return half4(0, .5, 0, .5); }
        )";
        static const char* kBlueCF = R"(
            half4 main(half4 color) { return half4(0, 0, .5, .5); }
        )";

        static const char* kCombineCF = R"(
            uniform colorFilter first;
            uniform colorFilter second;
            half4 main(half4 color) { return first.eval(color) + second.eval(color); }
        )";

        std::vector<sk_sp<PrecompileColorFilter>> combinations =
            create_runtime_combos<PrecompileColorFilter>(
                    reporter,
                    SkRuntimeEffect::MakeForColorFilter,
                    PrecompileRuntimeEffects::MakePrecompileColorFilter,
                    kRedCF,
                    kGreenCF,
                    kBlueCF,
                    kCombineCF);
        paintOptions.setColorFilters(combinations);
    }

    // blenders
    {
        static const char* kRedB = R"(
            half4 main(half4 src, half4 dst) { return half4(.5, 0, 0, .5); }
        )";
        static const char* kGreenB = R"(
            half4 main(half4 src, half4 dst) { return half4(0, .5, 0, .5); }
        )";
        static const char* kBlueB = R"(
            half4 main(half4 src, half4 dst) { return half4(0, 0, .5, .5); }
        )";

        static const char* kCombineB = R"(
            uniform blender first;
            uniform blender second;
            half4 main(half4 src, half4 dst) {
                return first.eval(src, dst) + second.eval(src, dst);
            }
        )";

        std::vector<sk_sp<PrecompileBlender>> combinations =
            create_runtime_combos<PrecompileBlender>(
                    reporter,
                    SkRuntimeEffect::MakeForBlender,
                    PrecompileRuntimeEffects::MakePrecompileBlender,
                    kRedB,
                    kGreenB,
                    kBlueB,
                    kCombineB);
        paintOptions.setBlenders(combinations);
    }

    REPORTER_ASSERT(reporter, paintOptions.priv().numCombinations() == 64);

    std::vector<UniquePaintParamsID> precompileIDs;
    paintOptions.priv().buildCombinations(keyContext,
                                          gatherer,
                                          DrawTypeFlags::kNone,
                                          /* withPrimitiveBlender= */ false,
                                          Coverage::kNone,
                                          [&precompileIDs](UniquePaintParamsID id,
                                                           DrawTypeFlags,
                                                           bool /* withPrimitiveBlender */,
                                                           Coverage) {
                                                               precompileIDs.push_back(id);
                                                           });

    SkASSERT(precompileIDs.size() == 64);
}

// Exercise all the PrecompileBlenders factories
void blend_subtest(const KeyContext& keyContext,
                   PipelineDataGatherer* gatherer,
                   skiatest::Reporter* reporter) {
    // The BlendMode PrecompileBlender only ever has 1 combination
    {
        PaintOptions paintOptions;
        paintOptions.setBlenders({ PrecompileBlenders::Mode(SkBlendMode::kColorDodge) });

        run_test(keyContext, gatherer, reporter, paintOptions, /* expectedNumOptions= */ 1);
    }

    // Specifying the BlendMode PrecompileBlender by SkBlendMode should also only ever
    // yield 1 combination.
    {
        PaintOptions paintOptions;
        paintOptions.setBlendModes({ SkBlendMode::kSrcOver });

        run_test(keyContext, gatherer, reporter, paintOptions, /* expectedNumOptions= */ 1);
    }

    // The Arithmetic PrecompileBlender only ever has 1 combination
    {
        PaintOptions paintOptions;
        paintOptions.setBlenders({ PrecompileBlenders::Arithmetic() });

        run_test(keyContext, gatherer, reporter, paintOptions, /* expectedNumOptions= */ 1);
    }
}

// Exercise all the PrecompileShaders factories
void shader_subtest(const KeyContext& keyContext,
                    PipelineDataGatherer* gatherer,
                    skiatest::Reporter* reporter) {
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::Empty() });

        run_test(keyContext, gatherer, reporter, paintOptions, /* expectedNumOptions= */ 1);
    }

    // The solid color shader only ever generates one combination. Because it is constant
    // everywhere it can cause other shaders to be elided (e.g., the LocalMatrix shader -
    // see the LocalMatrix test(s) below).
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::Color() });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedSolidColorCombos);
    }

    // In general, the blend shader generates the product of the options in each of its slots.
    // The rules for how many combinations the SkBlendModes yield are:
    //   all Porter-Duff SkBlendModes collapse to one option (see GetPorterDuffBlendConstants))
    //   all HSCL SkBlendModes collapse to another option
    //   all other SkBlendModes produce unique options
    {
        const SkBlendMode kBlendModes[] = {
                SkBlendMode::kSrcOut,    // Porter-Duff
                SkBlendMode::kSrcOver,   // Porter-Duff
                SkBlendMode::kHue,       // HSLC
                SkBlendMode::kColor,     // HSLC
                SkBlendMode::kScreen,    // Fixed Screen
                SkBlendMode::kDarken,    // fixed Darken
        };
        PaintOptions paintOptions;
        paintOptions.setShaders(
                { PrecompileShaders::Blend(SkSpan<const SkBlendMode>(kBlendModes),
                                           { PrecompileShaders::Color() },
                                           { PrecompileShaders::MakeFractalNoise() }) });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ 4 *  // Porter-Duff, HSLC, Screen, Darken
                                           kExpectedSolidColorCombos *
                                           kExpectedPerlinNoiseCombos);
    }

    // The ImageShaders have 6 combinations (3 sampling/tiling x 2 alpha/non-alpha)
    // The CoordClamp shader doesn't add any additional combinations to its wrapped shader.
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::CoordClamp({ PrecompileShaders::Image() }) });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedImageCombos);
    }

    // RawImageShaders only have 3 combinations (since they never incorporate alpha)
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::RawImage() });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedRawImageCombos);
    }

    // Each Perlin noise shader only has one combination
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::MakeFractalNoise(),
                                  PrecompileShaders::MakeTurbulence() });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedPerlinNoiseCombos + kExpectedPerlinNoiseCombos);
    }

    // Each gradient shader generates 3 combinations
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::LinearGradient(),
                                  PrecompileShaders::RadialGradient(),
                                  PrecompileShaders::TwoPointConicalGradient(),
                                  PrecompileShaders::SweepGradient() });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedGradientCombos + kExpectedGradientCombos +
                                           kExpectedGradientCombos + kExpectedGradientCombos);
    }

    // Each picture shader generates 12 combinations:
    //    2 (pictureShader LM) x 6 (imageShader variations)
    {
        PaintOptions paintOptions;
        paintOptions.setShaders({ PrecompileShaders::Picture() });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedPictureCombos);
    }

    // In general, the local matrix shader just generates however many options its wrapped
    // shader generates.
    {
        PaintOptions paintOptions;
        paintOptions.setShaders(
                { PrecompileShaders::LocalMatrix({ PrecompileShaders::LinearGradient() }) });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedGradientCombos);
    }

    // The ColorFilter shader just creates the cross product of its child options
    {
        PaintOptions paintOptions;
        paintOptions.setShaders(
                { PrecompileShaders::ColorFilter({ PrecompileShaders::LinearGradient() },
                                                 { PrecompileColorFilters::Blend() }) });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedGradientCombos * kExpectedBlendCFCombos);
    }

    {
        PaintOptions paintOptions;
        paintOptions.setShaders(
                { PrecompileShaders::WorkingColorSpace({ PrecompileShaders::LinearGradient() },
                                                       { SkColorSpace::MakeSRGBLinear() }) });

        run_test(keyContext, gatherer, reporter, paintOptions,
                 /* expectedNumOptions= */ kExpectedGradientCombos *
                                           1 /* only one colorSpace */);
    }
}

// Exercise all the PrecompileColorFilters factories. The impact of colorfilters on the number
// of combinations is very predictable. Except for the Compose and Lerp color filters, all the
// color filters only ever have one combination. The Compose and Lerp color filters also just
// simply generate the cross product of their children.
void colorfilter_subtest(const KeyContext& keyContext,
                         PipelineDataGatherer* gatherer,
                         skiatest::Reporter* reporter) {


    {
        // The compose colorfilter just generates the cross product of its children.
        static constexpr int kExpectedNumOptions =
                kExpectedTableCFCombos * (kExpectedHighContrastCFCombos + kExpectedLumaCFCombos) +
                kExpectedLightingCFCombos * (kExpectedHighContrastCFCombos + kExpectedLumaCFCombos);

        PaintOptions paintOptions;
        paintOptions.setColorFilters(
            { PrecompileColorFilters::Compose(
                    { PrecompileColorFilters::Table(), PrecompileColorFilters::Lighting() },
                    { PrecompileColorFilters::HighContrast(), PrecompileColorFilters::Luma() }) });

        run_test(keyContext, gatherer, reporter, paintOptions, kExpectedNumOptions);
    }

    {
        PaintOptions paintOptions;
        paintOptions.setColorFilters({ PrecompileColorFilters::Blend() });

        run_test(keyContext, gatherer, reporter, paintOptions, kExpectedBlendCFCombos);
    }

    {
        PaintOptions paintOptions;
        paintOptions.setColorFilters({ PrecompileColorFilters::Matrix(),
                                       PrecompileColorFilters::HSLAMatrix() });

        // HSLAMatrix and Matrix map to the same color filter
        run_test(keyContext, gatherer, reporter, paintOptions,
                 kExpectedMatrixCFCombos + kExpectedMatrixCFCombos);
    }

    {
        PaintOptions paintOptions;
        paintOptions.setColorFilters({ PrecompileColorFilters::LinearToSRGBGamma(),
                                       PrecompileColorFilters::SRGBToLinearGamma() });

        // LinearToSRGB and SRGBToLinear both map to the colorspace colorfilter
        run_test(keyContext, gatherer, reporter, paintOptions,
                 kExpectedColorSpaceCFCombos + kExpectedColorSpaceCFCombos);
    }

    {
        // The lerp colorfilter just generates the cross product of its children.
        static constexpr int kExpectedNumOptions =
                kExpectedMatrixCFCombos * (kExpectedBlendCFCombos + kExpectedOverdrawCFCombos) +
                kExpectedLumaCFCombos * (kExpectedBlendCFCombos + kExpectedOverdrawCFCombos);

        PaintOptions paintOptions;
        paintOptions.setColorFilters(
            { PrecompileColorFilters::Lerp(
                    { PrecompileColorFilters::Matrix(), PrecompileColorFilters::Luma() },
                    { PrecompileColorFilters::Blend(), PrecompileColorFilters::Overdraw() }) });

        run_test(keyContext, gatherer, reporter, paintOptions, kExpectedNumOptions);
    }
}

} // anonymous namespace

DEF_GRAPHITE_TEST_FOR_ALL_CONTEXTS(CombinationBuilderTest, reporter, context,
                                   CtsEnforcement::kNextRelease) {
    ShaderCodeDictionary* dict = context->priv().shaderCodeDictionary();

    auto rtEffectDict = std::make_unique<RuntimeEffectDictionary>();

    SkColorInfo ci(kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
    KeyContext keyContext(context->priv().caps(),
                          dict,
                          rtEffectDict.get(),
                          ci,
                          /* dstTexture= */ nullptr,
                          /* dstOffset= */ {0, 0});

    PipelineDataGatherer gatherer(Layout::kMetal);

    // The default PaintOptions should create a single combination with a solid color shader and
    // kSrcOver blending
    {
        PaintOptions paintOptions;

        run_test(keyContext, &gatherer, reporter, paintOptions, /* expectedNumOptions= */ 1);
    }

    blend_subtest(keyContext, &gatherer, reporter);
    shader_subtest(keyContext, &gatherer, reporter);
    colorfilter_subtest(keyContext, &gatherer, reporter);

    no_blend_mode_option_test(keyContext, &gatherer, reporter);
    big_test(keyContext, &gatherer, reporter);
    runtime_effect_test(keyContext, &gatherer, reporter);
}

#endif // SK_GRAPHITE
