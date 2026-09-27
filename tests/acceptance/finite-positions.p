import Nat;
import Fin;
import fin_to_nat;
import fin_weaken;
import fin_flip;
import same_position;
import position_permutation;
import position_forward;
import position_backward;
import position_left_inverse;
import position_right_inverse;
import position_identity;
import position_flip;
import position_inverse;
import position_compose;
import position_keep;
import tuple_permute;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
first := Fin.zero one;
second := Fin.succ (Fin.zero zero);

flip := fin_flip two;
flip :: Fin two->Fin two;
swap := position_flip two;
swap :: position_permutation two;
main := fin_to_nat two (position_forward two swap first);
back := fin_to_nat two (position_backward two (position_inverse two swap) first);
twice := fin_to_nat two (position_forward two (position_compose two swap swap) first);
weakened := fin_to_nat three (fin_weaken two second);
roundtrip := position_left_inverse two swap first;
roundtrip :: same_position two first first;
other_roundtrip := position_right_inverse two swap second;
other_roundtrip :: same_position two second second;

// Equal payloads retain two distinct positions. Labels are the positions here.
payload := \i:Fin two => Nat.zero;
duplicate_payload := tuple_permute Nat two &payload swap first;
label := tuple_permute (Fin two) two &(\i:Fin two => i) swap first;
label_number := fin_to_nat two label;

empty := position_flip zero;
empty :: position_permutation zero;
singleton := fin_to_nat one (position_forward one (position_flip one) (Fin.zero zero));
third := Fin.succ (Fin.succ (Fin.zero zero));
unchanged := fin_to_nat three (position_forward three (position_flip three) third);
identity := fin_to_nat three (position_forward three (position_identity three) third);
kept := position_keep two swap;
keep_first := fin_to_nat three (position_forward three kept (Fin.zero two));
keep_second := fin_to_nat three (position_forward three kept (Fin.succ (Fin.zero one)));
keep_third := fin_to_nat three (position_forward three kept third);
cycle := position_compose three (position_flip three) kept;
cycle_first := fin_to_nat three (position_forward three cycle (Fin.zero two));
cycle_second := fin_to_nat three (position_forward three cycle (Fin.succ (Fin.zero one)));
cycle_third := fin_to_nat three (position_forward three cycle third);
keep_empty := position_keep zero (position_identity zero);
keep_empty_value := fin_to_nat one (position_forward one keep_empty (Fin.zero zero));

general_roundtrip := \n:Nat => \p:position_permutation n => \q:position_permutation n =>
	\i:Fin n => position_left_inverse n (position_compose n p q) i;
general_roundtrip :: (n:Nat)->(p:position_permutation n)->(q:position_permutation n)->(i:Fin n)->
	same_position n (position_backward n (position_compose n p q)
		(position_forward n (position_compose n p q) i)) i;
general_keep := \n:Nat => \p:position_permutation n => \i:Fin (Nat.succ n) =>
	position_left_inverse (Nat.succ n) (position_keep n p) i;
general_keep :: (n:Nat)->(p:position_permutation n)->(i:Fin (Nat.succ n))->
	same_position (Nat.succ n) (position_backward (Nat.succ n) (position_keep n p)
		(position_forward (Nat.succ n) (position_keep n p) i)) i;
