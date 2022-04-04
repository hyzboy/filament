/*
 * Copyright 2022 The Android Open Source Project
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

#include <ktxreader/Ktx1Reader.h>

#include <gtest/gtest.h>
#include <utils/Path.h>

#include <fstream>

using std::string;
using utils::Path;

class KtxReaderTest : public testing::Test {};

static string readFile(const Path& inputPath) {
    std::ifstream t(inputPath);
    string s;

    // Pre-allocate the memory
    t.seekg(0, std::ios::end);
    s.reserve((size_t) t.tellg());
    t.seekg(0, std::ios::beg);

    // Copy the file content into the string
    s.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    return s;
}

TEST_F(KtxReaderTest, Ktx1) {
    const utils::Path parent = Path::getCurrentExecutable().getParent();
    const string contents = readFile(parent + "color_grid_uastc_zstd.ktx2");
    ASSERT_EQ(contents.size(), 170512);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
