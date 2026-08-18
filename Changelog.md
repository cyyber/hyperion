### 0.8.24

Breaking Changes:
 * Type System: ``int`` and ``uint`` are now aliases for ``int512`` and ``uint512``, i.e. they denote the full QRVM word. Explicitly sized types such as ``int256`` and ``uint256`` keep their declared width.
 * Type System: Array indices, array slice bounds and the length argument of ``new T[](n)`` are full-word quantities now, which also widens the index parameter of automatically generated array getters.
 * ABI: The built-in ``Panic`` error is now ``Panic(uint512)`` and its selector changed to ``0xdf75fb3b``. The payload layout is unchanged, since it always was a four byte selector followed by a single VM word. ``Error(string)`` is unaffected.
