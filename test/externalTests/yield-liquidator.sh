#!/usr/bin/env bash

# ------------------------------------------------------------------------------
# This file is part of hyperion.
#
# hyperion is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# hyperion is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with hyperion.  If not, see <http://www.gnu.org/licenses/>
#
# (c) 2022 hyperion contributors.
#------------------------------------------------------------------------------

set -e

source scripts/common.sh
source scripts/externalTests/common.sh

REPO_ROOT=$(realpath "$(dirname "$0")/../..")

verify_input "$@"
BINARY_TYPE="$1"
BINARY_PATH="$(realpath "$2")"
SELECTED_PRESETS="$3"

function compile_fn { npm run build; }
function test_fn { npm run test; }

function yield_liquidator_test
{
    local repo="https://github.com/yieldprotocol/yield-liquidator-v2"
    local ref_type=branch
    local ref="master"
    local config_file="hardhat.config.ts"
    local config_var="module.exports"

    local compile_only_presets=()
    local settings_presets=(
        "${compile_only_presets[@]}"
        ir-no-optimize
        ir-optimize-zvm-only
        ir-optimize-zvm+yul
        legacy-optimize-zvm-only
        legacy-optimize-zvm+yul
        legacy-no-optimize
    )

    [[ $SELECTED_PRESETS != "" ]] || SELECTED_PRESETS=$(circleci_select_steps_multiarg "${settings_presets[@]}")
    print_presets_or_exit "$SELECTED_PRESETS"

    setup_hypc "$DIR" "$BINARY_TYPE" "$BINARY_PATH"
    download_project "$repo" "$ref_type" "$ref" "$DIR"

    neutralize_package_lock
    neutralize_package_json_hooks
    force_hardhat_compiler_binary "$config_file" "$BINARY_TYPE" "$BINARY_PATH"
    force_hardhat_compiler_settings "$config_file" "$(first_word "$SELECTED_PRESETS")" "$config_var"
    force_hardhat_unlimited_contract_size "$config_file" "$config_var"
    npm install

    # The contract below is not used in any test and it depends on ISwapRouter which does not exists
    # in the main repository.
    # See: https://github.com/yieldprotocol/yield-liquidator-v2/blob/9a49d9a0e9398f6a6c07bad531e77d1001a1166f/src/swap_router.rs#L94
    rm --force contracts/.YvBasicFlashLiquidator.hyp

    replace_version_pragmas
    neutralize_packaged_contracts

    for preset in $SELECTED_PRESETS; do
        hardhat_run_test "$config_file" "$preset" "${compile_only_presets[*]}" compile_fn test_fn "$config_var"
        store_benchmark_report hardhat yield_liquidator "$repo" "$preset"
    done
}

external_test Yield-Liquidator-V2 yield_liquidator_test
