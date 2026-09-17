import Nat;
import Vec;
import length;
import one;
import two;
import prepend;
import quoted;
import constructor;

nil := (Vec Nat).nil;
single := constructor Nat.zero nil;
double := prepend single;
triple := quoted double;
main := length Nat (Nat.succ two) triple;
expected := Nat.succ two;
main :: Nat;

identity := \n:Nat => \xs:Vec Nat n => xs @nil => nil
	@cons head tail => (Vec Nat).cons head (*tail);
copyMain := length Nat two (identity two double);
copyExpected := two;
