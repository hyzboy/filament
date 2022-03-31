/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ktxreader/Ktx2Reader.h>

#include <filament/Engine.h>
#include <filament/Texture.h>

#include <utils/Log.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warray-bounds"
#include <basisu_transcoder.h>
#pragma clang diagnostic pop

using namespace basist;
using namespace filament;

using Transform = ktxreader::Ktx2Reader::Transform;

namespace {
struct FinalFormatInfo {
    bool isSupported;
    bool isCompressed;
    Transform transformFunction;
    transcoder_texture_format basisFormat;
    Texture::CompressedType compressedPixelDataType;
    Texture::Type pixelDataType;
    Texture::Format pixelDataFormat;
};
}

// Return by value isn't expensive here due to copy elision.
static FinalFormatInfo getFinalFormatInfo(Texture::InternalFormat fmt) {
    using tif = Texture::InternalFormat;
    using tct = Texture::CompressedType;
    using tt = Texture::Type;
    using tf = Texture::Format;
    using ttf = transcoder_texture_format;
    const auto sRGB = ktxreader::Ktx2Reader::sRGB;
    const auto LINEAR = ktxreader::Ktx2Reader::LINEAR;
    switch (fmt) {
        case tif::ETC2_EAC_SRGBA8: return {true, true, sRGB, ttf::cTFETC2_RGBA, tct::ETC2_EAC_RGBA8};
        case tif::ETC2_EAC_RGBA8:  return {true, true, LINEAR, ttf::cTFETC2_RGBA, tct::ETC2_EAC_SRGBA8};
        case tif::DXT1_SRGB: return {true, true, sRGB, ttf::cTFBC1_RGB, tct::DXT1_RGB};
        case tif::DXT1_RGB: return {true, true, LINEAR, ttf::cTFBC1_RGB, tct::DXT1_SRGB};
        case tif::DXT3_SRGBA: return {true, true, sRGB, ttf::cTFBC3_RGBA, tct::DXT3_RGBA};
        case tif::DXT3_RGBA: return {true, true, LINEAR, ttf::cTFBC3_RGBA, tct::DXT3_SRGBA};
        case tif::SRGB8_ALPHA8_ASTC_4x4: return {true, true, sRGB, ttf::cTFASTC_4x4_RGBA, tct::RGBA_ASTC_4x4};
        case tif::RGBA_ASTC_4x4: return {true, true, LINEAR, ttf::cTFASTC_4x4_RGBA, tct::SRGB8_ALPHA8_ASTC_4x4};
        case tif::EAC_R11: return {true, true, LINEAR, ttf::cTFETC2_EAC_R11, tct::EAC_R11};

        // The following format is useful for normal maps.
        // Note that BasisU supports only the unsigned variant.
        case tif::EAC_RG11: return {true, true, LINEAR, ttf::cTFETC2_EAC_RG11, tct::EAC_RG11};

        // Uncompressed formats.
        case tif::SRGB8_A8: return {true, false, sRGB, ttf::cTFRGBA32, {}, tt::UBYTE, tf::RGBA};
        case tif::RGBA8: return {true, false, LINEAR, ttf::cTFRGBA32, {}, tt::UBYTE, tf::RGBA};
        case tif::RGB565: return {true, false, LINEAR, ttf::cTFRGB565, {}, tt::USHORT_565, tf::RGB};
        case tif::RGBA4: return {true, false, LINEAR, ttf::cTFRGBA4444, {}, tt::USHORT, tf::RGBA};

        default: return {false};
    }
}

// This function converts a Filament format enumerant into a BasisU enumerant.
//
// Note that Filament's internal format list mimics the Vulkan format list, which
// embeds transfer function information (i.e. sRGB or not) into the format, whereas
// the basis format list does not.
//
// The following formats supported by BasisU but are not supported by Filament.
//
//     transcoder_texture_format::cTFETC1_RGB
//     transcoder_texture_format::cTFATC_RGB
//     transcoder_texture_format::cTFATC_RGBA
//     transcoder_texture_format::cTFFXT1_RGB
//     transcoder_texture_format::cTFPVRTC2_4_RGB
//     transcoder_texture_format::cTFPVRTC2_4_RGBA
//     transcoder_texture_format::cTFPVRTC1_4_RGB
//     transcoder_texture_format::cTFPVRTC1_4_RGBA
//     transcoder_texture_format::cTFBC4_R
//     transcoder_texture_format::cTFBC5_RG
//     transcoder_texture_format::cTFBC7_RGBA (this format would add size bloat to the transcoder)
//     transcoder_texture_format::cTFBGR565   (note the blue/red swap)
//
static bool convertFormat(Texture::InternalFormat fmt, transcoder_texture_format* dest,
        Transform* pTransform = nullptr) {
    using Fmt = Texture::InternalFormat;
    Transform transform;
    if (!pTransform) {
        pTransform = &transform;
    }
    switch (fmt) {
        case Fmt::ETC2_EAC_SRGBA8:
            *pTransform = ktxreader::Ktx2Reader::sRGB;
            *dest = transcoder_texture_format::cTFETC2_RGBA;
            return true;
        case Fmt::ETC2_EAC_RGBA8:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFETC2_RGBA;
            return true;

        case Fmt::DXT1_SRGB:
            *pTransform = ktxreader::Ktx2Reader::sRGB;
            *dest = transcoder_texture_format::cTFBC1_RGB;
            return true;
        case Fmt::DXT1_RGB:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFBC1_RGB;
            return true;

        case Fmt::DXT3_SRGBA:
            *pTransform = ktxreader::Ktx2Reader::sRGB;
            *dest = transcoder_texture_format::cTFBC3_RGBA;
            return true;
        case Fmt::DXT3_RGBA:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFBC3_RGBA;
            return true;

        case Fmt::SRGB8_ALPHA8_ASTC_4x4:
            *pTransform = ktxreader::Ktx2Reader::sRGB;
            *dest = transcoder_texture_format::cTFASTC_4x4_RGBA;
            return true;
        case Fmt::RGBA_ASTC_4x4:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFASTC_4x4_RGBA;
            return true;

        case Fmt::EAC_R11:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFETC2_EAC_R11;
            return true;

        // The following format is useful for normal maps.
        // Note that BasisU supports only the unsigned variant.
        case Fmt::EAC_RG11:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFETC2_EAC_RG11;
            return true;

        case Fmt::SRGB8_A8:
            *pTransform = ktxreader::Ktx2Reader::sRGB;
            *dest = transcoder_texture_format::cTFRGBA32;
            return true;
        case Fmt::RGBA8:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFRGBA32;
            return true;

        case Fmt::RGB565:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFRGB565;
            return true;

        case Fmt::RGBA4:
            *pTransform = ktxreader::Ktx2Reader::LINEAR;
            *dest = transcoder_texture_format::cTFRGBA4444;
            return true;

        default: break;
    }
    return false;
}

namespace ktxreader {

Ktx2Reader::Ktx2Reader(Engine& engine, bool quiet) :
    mEngine(engine),
    mQuiet(quiet),
    mTranscoder(new ktx2_transcoder()) {
    mRequestedFormats.reserve((size_t) transcoder_texture_format::cTFTotalTextureFormats);
    basisu_transcoder_init();
}

Ktx2Reader::~Ktx2Reader() {
    delete mTranscoder;
}

bool Ktx2Reader::requestFormat(Texture::InternalFormat format) {
    transcoder_texture_format dest;
    if (!convertFormat(format, &dest)) {
        return false;
    }
    for (Texture::InternalFormat fmt : mRequestedFormats) {
        if (fmt == format) {
            return false;
        }
    }
    mRequestedFormats.push_back(format);
    return true;
}

void Ktx2Reader::unrequestFormat(Texture::InternalFormat format) {
    for (auto iter = mRequestedFormats.begin(); iter != mRequestedFormats.end(); ++iter) {
        if (*iter == format) {
            mRequestedFormats.erase(iter);
            return;
        }
    }
}

Texture* Ktx2Reader::load(const uint8_t* data, size_t size, Transform transform) {
    if (!mTranscoder->init(data, size)) {
        if (!mQuiet) {
            utils::slog.e << "BasisU transcoder init failed." << utils::io::endl;
        }
        return nullptr;
    }

    // TODO: check that mTranscoder->get_dfd_transfer_func() matches expectations.
    if (!mTranscoder->start_transcoding()) {
        if (!mQuiet) {
            utils::slog.e << "BasisU start_transcoding failed." << utils::io::endl;
        }
        return nullptr;
    }

    // TODO: support cubemaps. For now we use KTX1 for cubemaps because basisu does not support HDR.
    if (mTranscoder->get_faces() == 6) {
        if (!mQuiet) {
            utils::slog.e << "Cubemaps are not yet supported." << utils::io::endl;
        }
        return nullptr;
    }

    // TODO: support texture arrays.
    if (mTranscoder->get_layers() > 1) {
        if (!mQuiet) {
            utils::slog.e << "Texture arrays are not yet supported." << utils::io::endl;
        }
        return nullptr;
    }

    // Fierst pass through, just to make sure we can transcode it.
    bool found = false;
    Texture::InternalFormat resolvedFormat;
    for (Texture::InternalFormat requestedFormat : mRequestedFormats) {
        transcoder_texture_format basisFormat;
        Transform impliedTransform;
        if (!convertFormat(requestedFormat, &basisFormat, &impliedTransform)) {
            continue;
        }
        if (impliedTransform != transform) {
            continue;
        }
        if (!basis_is_format_supported(basisFormat, mTranscoder->get_format())) {
            continue;
        }
        if (!Texture::isTextureFormatSupported(mEngine, requestedFormat)) {
            continue;
        }
        const uint32_t layerIndex = 0;
        const uint32_t faceIndex = 0;
        for (uint32_t levelIndex = 0; levelIndex < mTranscoder->get_levels(); levelIndex++) {
            basist::ktx2_image_level_info info;
            if (!mTranscoder->get_image_level_info(info, levelIndex, layerIndex, faceIndex)) {
                continue;
            }
        }
        found = true;
        resolvedFormat = requestedFormat;
        break;
    }

    if (!found) {
        if (!mQuiet) {
            utils::slog.e << "Unable to decode any of the requested formats." << utils::io::endl;
        }
        return nullptr;
    }

    transcoder_texture_format basisFormat;
    convertFormat(resolvedFormat, &basisFormat);

    Texture* texture = Texture::Builder()
        .width(mTranscoder->get_width())
        .height(mTranscoder->get_height())
        .levels(mTranscoder->get_levels())
        .sampler(Texture::Sampler::SAMPLER_2D)
        .format(resolvedFormat)
        .build(mEngine);

    // TODO: set Texture::Type (for the no-compressed ones)
    // TODO: Merge this with the function at the top, call it "getFormatInfo" or something.
    using Fmt = Texture::InternalFormat;
    Texture::CompressedType cdatatype;
    Texture::Type ucdatatype;
    bool isCompressed = true;
    switch (resolvedFormat) {
        case Fmt::ETC2_EAC_RGBA8: cdatatype = Texture::CompressedType::ETC2_EAC_RGBA8; break;
        case Fmt::ETC2_EAC_SRGBA8: cdatatype = Texture::CompressedType::ETC2_EAC_SRGBA8; break;
        case Fmt::DXT1_RGB: cdatatype = Texture::CompressedType::DXT1_RGB; break;
        case Fmt::DXT1_SRGB: cdatatype = Texture::CompressedType::DXT1_SRGB; break;
        case Fmt::DXT3_RGBA: cdatatype = Texture::CompressedType::DXT3_RGBA; break;
        case Fmt::DXT3_SRGBA: cdatatype = Texture::CompressedType::DXT3_SRGBA; break;
        case Fmt::RGBA_ASTC_4x4: cdatatype = Texture::CompressedType::RGBA_ASTC_4x4; break;
        case Fmt::SRGB8_ALPHA8_ASTC_4x4: cdatatype = Texture::CompressedType::SRGB8_ALPHA8_ASTC_4x4; break;
        case Fmt::EAC_R11: cdatatype = Texture::CompressedType::EAC_R11; break;
        case Fmt::EAC_RG11: cdatatype = Texture::CompressedType::EAC_RG11; break;

        case Fmt::SRGB8_A8:
        case Fmt::RGBA8:
        case Fmt::RGB565:
        case Fmt::RGBA4:
            // TODO: set ucdatatype
            isCompressed = false;
            assert(false && "TODO: Uncompressed types not yet implemented");
            break;
        default:
            assert(false && "Unreachable due to the earlier pass through the requested formats.");
    }

    // In theory we could pass "free" directly into the callback but doing so triggers
    // ASAN warnings and WASM compiler issues.
    Texture::PixelBufferDescriptor::Callback cb = [](void* buf, size_t, void* userdata) {
        free(buf);
    };

    const uint32_t layerIndex = 0;
    const uint32_t faceIndex = 0;
    for (uint32_t levelIndex = 0; levelIndex < mTranscoder->get_levels(); levelIndex++) {
        basist::ktx2_image_level_info info;
        mTranscoder->get_image_level_info(info, levelIndex, layerIndex, faceIndex);
        const basisu::texture_format destFormat = basis_get_basisu_texture_format(basisFormat);
        const uint32_t qwordsPerBlock = basisu::get_qwords_per_block(destFormat);
        const size_t byteCount = sizeof(uint64_t) * qwordsPerBlock * info.m_total_blocks;
        uint64_t* const blocks = (uint64_t*) malloc(byteCount);
        const uint32_t flags = 0;
        if (!mTranscoder->transcode_image_level(levelIndex, layerIndex, faceIndex, blocks,
                info.m_total_blocks, basisFormat, flags)) {
            utils::slog.e << "Failed to transcode level " << levelIndex << utils::io::endl;
            return nullptr;
        }
        Texture::PixelBufferDescriptor pbd(blocks, byteCount, cdatatype, byteCount, cb, nullptr);
        texture->setImage(mEngine, levelIndex, std::move(pbd));
    }

    return texture;
}

} // namespace ktxreader
