{
    function f() -> a {}
    let b := add(7, sub(f(), 7))
    mstore(b, 0)
}
// ----
// step: fullSimplify
//
// {
//     {
//         mstore(add(f(), 0x010000000000000000000000000000000000000000000000000000000000000000), 0)
//     }
//     function f() -> a
//     { }
// }
