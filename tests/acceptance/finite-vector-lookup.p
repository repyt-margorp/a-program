import Nat;
import Fin;
import fin_to_nat;
import Vec;
import vec_lookup;
import vec_tabulate;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
sample := (Vec Nat).cons one ((Vec Nat).cons zero (Vec Nat).nil);
lookup_first := vec_lookup Nat two sample (Fin.zero one);
lookup_second := vec_lookup Nat two sample (Fin.succ (Fin.zero zero));
tabulated := vec_tabulate Nat two &(\i:Fin two => fin_to_nat two i);
main := vec_lookup Nat two tabulated (Fin.succ (Fin.zero zero));
