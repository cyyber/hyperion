{
  mstore(0, balance(address()))
  mstore(0x20, selfbalance())
  mstore(0x40, balance(div(mul(address(), 2), 2)))
  mstore(0x60, balance(add(address(), 1)))
}
// ----
// Trace:
// Memory dump:
//     80: 00000000000000000000000000000000000000000000000000000000222222220000000000000000000000000000000000000000000000000000000000000000
// Storage dump:
