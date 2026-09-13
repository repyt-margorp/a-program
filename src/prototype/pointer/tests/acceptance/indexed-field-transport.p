Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
One := @\n:Nat => {at:(k:Nat)->* (Nat.succ k);};
use := \n:Nat => \f:Witness n -> Nat => \v:One (Nat.succ n) =>
	v @at k => f (Witness.at k);
use :: (n:Nat) -> (Witness n -> Nat) -> One (Nat.succ n) -> Nat;
read := \w:Witness (Nat.succ Nat.zero) => w @at k => k;
expected := Nat.succ Nat.zero;
main := use expected &read (One.at expected);
Pair := @{mk:Nat->Nat->*;};
Row := @\p:Pair => {at:(k:Nat)->(j:Nat)->* (Pair.mk k j);};
usePair := \n:Nat => \m:Nat => \f:Witness n -> Nat => \v:Row (Pair.mk n m) =>
	v @at k j => f (Witness.at k);
pairMain := usePair expected Nat.zero &read (Row.at expected Nat.zero);
