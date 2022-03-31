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

#ifndef IMAGE_BASISENCODER_H_
#define IMAGE_BASISENCODER_H_

#include <stddef.h>
#include <stdint.h>
#include <utils/compiler.h>

#include <image/LinearImage.h>

namespace image {

struct BasisEncoderBuilderImpl;
struct BasisEncoderImpl;

class UTILS_PUBLIC BasisEncoder {
public:
    class Builder {
    public:
        Builder(size_t mipCount) noexcept;
        ~Builder() noexcept;
        Builder(Builder&& that) noexcept;
        Builder& operator=(Builder&& that) noexcept;

        Builder& grayscale(bool enabled) noexcept;
        Builder& linear(bool enabled) noexcept;
        Builder& normals(bool enabled) noexcept;
        Builder& miplevel(size_t mipIndex, const LinearImage& image) noexcept;
        Builder& jobs(size_t count) noexcept;
        Builder& quiet(bool enabled) noexcept;

        BasisEncoder* build();

    private:
        BasisEncoderBuilderImpl* mImpl;
        Builder(const Builder&) = delete;
        Builder& operator=(const Builder&) = delete;
    };

    ~BasisEncoder() noexcept;
    BasisEncoder(BasisEncoder&& that) noexcept;
    BasisEncoder& operator=(BasisEncoder&& that) noexcept;

    bool encode();

    size_t getKtx2ByteCount() const noexcept;
    uint8_t const* getKtx2Data() const noexcept;

private:
    BasisEncoder(BasisEncoderImpl*) noexcept;
    BasisEncoder(const BasisEncoder&) = delete;
    BasisEncoder& operator=(const BasisEncoder&) = delete;
    BasisEncoderImpl* mImpl;
    friend struct BasisEncoderBuilderImpl;
};

} // namespace image

#endif // IMAGE_BASISENCODER_H_
