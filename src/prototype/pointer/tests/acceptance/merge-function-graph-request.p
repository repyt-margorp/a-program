// Use merge-function-graph-composition.p as --imports.
import curriedMerge;
import Nat;
import lessEqual;
import left;
import right;
import expected;
graph := @curriedMerge;
witness := *curriedMerge;
main := *curriedMerge Nat &lessEqual left right @output => output;
graphExpected := expected;
