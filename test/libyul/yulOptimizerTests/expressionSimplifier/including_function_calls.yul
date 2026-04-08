{
    function f() -> a {}
    let b := add(7, sub(f(), 7))
    sstore(0, b)
}
// ----
// step: expressionSimplifier
//
// {
//     {
//         sstore(0, add(f(), 0x010000000000000000000000000000000000000000000000000000000000000000))
//     }
//     function f() -> a
//     { }
// }
