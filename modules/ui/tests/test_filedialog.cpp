/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>

#include <lagrange/Logger.h>
#include <lagrange/ui/utils/file_dialog.h>

namespace ui = lagrange::ui;


TEST_CASE("FileDialogUtils", "[ui][filedialog]")
{
    SECTION("single")
    {
        REQUIRE(
            ui::utils::transform_filters_to_accept(
                {ui::FileFilter{"label", "*.png *.jpg *.jpeg *.bmp"}}) == ".png,.jpg,.jpeg,.bmp");
        REQUIRE(
            ui::utils::transform_filters_to_accept(
                {ui::FileFilter{"label", "*.png,*.jpg   ,*.jpeg, *.bmp"}}) ==
            ".png,.jpg,.jpeg,.bmp");

        REQUIRE(ui::utils::transform_filters_to_accept({ui::FileFilter{"all", "*"}}) == "");
    }


    SECTION("multiple")
    {
        REQUIRE(
            ui::utils::transform_filters_to_accept(
                {{"pngs", "*.png"}, {"jpgs", "*.jpg, *.jpeg"}, {"bmps", "*.bmp"}}) ==
            ".png,.jpg,.jpeg,.bmp");

        // All * wildcard
        REQUIRE(
            ui::utils::transform_filters_to_accept(
                {{"pngs", "*.png"}, {"jpgs", "*.jpg, *.jpeg"}, {"bmps", "*.bmp"}, {"All", "*"}}) ==
            "");
    }

    SECTION("mime")
    {
        REQUIRE(
            ui::utils::transform_filters_to_accept(
                {{"mix", "*.wav,audio/* .lk video/*,.data, image/*, .x,image/png"}}) ==
            ".wav,audio/*,.lk,video/*,.data,image/*,.x,image/png");
    }
}
