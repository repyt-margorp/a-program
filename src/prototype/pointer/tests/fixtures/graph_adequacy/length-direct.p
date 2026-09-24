Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
adequacy := \xs:List => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
adequacy :: (xs:List) -> @length xs (length xs);
Unary := @\n:Nat => {
	zero:* Nat.zero;
	succ:(k:Nat)->* k->* (Nat.succ k);
};
property := \xs:List => \n:Nat => \graph:@length xs n => graph
	@nil => Unary.zero
	@cons head tail count rest => Unary.succ count *rest;
property_at_result := \xs:List => property xs (length xs) (adequacy xs);
property_at_result :: (xs:List) -> Unary (length xs);
count_proof := \n:Nat => \proof:Unary n => proof
	@zero => Nat.zero
	@succ k rest => Nat.succ *rest;
two := Nat.succ (Nat.succ Nat.zero);
input := List.cons two (List.cons Nat.zero List.nil);
main := count_proof (length input) (property_at_result input);
