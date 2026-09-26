// Use merge-function-graph-composition.p as --imports.
import curriedMerge;
import Nat;
import lessEqual;
import left;
import right;
import expected;
graph := @curriedMerge;
main := curriedMerge Nat &lessEqual left right;
graphExpected := expected;
