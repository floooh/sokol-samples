Just check if the isolated implementations compile both as C and C++
(or ObjC / ObjC++).

NOTE: the file and build structure is so "verbose" to make really sure that
implementation includes don't accidentally cross-pollinate the
implementation of the header being compile-tested.

In real-world-projects it often makes more sense to include all
implementations into the same source file.
