#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=scripts/common.sh
source "${REPO_ROOT}/scripts/common.sh"

tmpdir=$(mktemp -d -t "cmdline-test-extfn-array-copy-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT

source_file="${tmpdir}/external_function_array_storage_copy.hyp"
asm_file="${tmpdir}/external_function_array_storage_copy.asm"

cat > "$source_file" <<'EOF'
contract DynamicCopy {
    function() external[] a;
    function() external[] b;

    function f() public {
        a = b;
    }
}

contract FixedCopy {
    function() external[3] a;
    function() external[3] b;

    function f() public {
        a = b;
    }
}
EOF

msg_on_error --no-stderr "$HYPC" --asm "$source_file" > "$asm_file"

normalized_asm=$(tr '\n' ' ' < "$asm_file" | tr -s '[:space:]' ' ')

# Regression check for legacy storage-to-storage copies of function() external[].
# External function pointers occupy two storage slots on QRVM: address + selector.
# The loop must therefore load/store slot+1 and advance both pointers by 2 slots.
expected_selector_copy='dup3 dup1 sload swap1 0x01 add sload 0xffffffff and dup4 dup1 0x01 add dup3 swap1 sstore swap1 pop sstore swap2 0x02 add swap2 swap1 0x02 add'
remaining_asm="$normalized_asm"
copy_count=0
while [[ "$remaining_asm" == *"$expected_selector_copy"* ]]
do
    copy_count=$((copy_count + 1))
    remaining_asm="${remaining_asm#*"$expected_selector_copy"}"
done

if (( copy_count < 2 ))
then
    printError "Legacy storage-to-storage copy of function() external[] did not copy the selector slot."
    printError "Expected selector-copy sequence at least twice, for dynamic and fixed-size arrays, but found ${copy_count}."
    printError "Expected assembly sequence:"
    >&2 echo "$expected_selector_copy"
    printError "Actual assembly:"
    >&2 cat "$asm_file"
    fail
fi
