{
	for {} mstore(1, 1) {} {}
}
// ====
// dialect: zvm
// ----
// TypeError 3950: (10-22): Expected expression to evaluate to one value, but got 0 values instead.
