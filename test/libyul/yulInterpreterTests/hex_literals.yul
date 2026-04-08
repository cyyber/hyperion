{
	let x := hex"112233445566778899aabbccddeeff6677889900"
	let y := hex"1234_abcd"
	sstore(0, x)
	sstore(1, y)
}
// ----
// Trace:
// Memory dump:
// Storage dump:
