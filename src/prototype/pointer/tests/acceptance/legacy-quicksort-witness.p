import Nat;
import List;
import SizedList;
import lessOrEqual;
import natAccessible;
import quickSortAcc;

one := Nat.succ Nat.zero;
two := Nat.succ one;
input := (SizedList Nat).cons one two ((SizedList Nat).cons Nat.zero one (SizedList Nat).nil);
graph := @quickSortAcc;
main := quickSortAcc Nat &lessOrEqual two (natAccessible two) input;
expected := (List Nat).cons one ((List Nat).cons two (List Nat).nil);
emptyMain := quickSortAcc Nat &lessOrEqual Nat.zero (natAccessible Nat.zero) (SizedList Nat).nil;
emptyExpected := (List Nat).nil;
