Unit := @{unit:*;};
Counter := \A : @ => @{zero:*; succ:*->*;};
Nat := Counter Unit;
main := Nat.succ Nat.zero;
expected := (Counter Unit).succ (Counter Unit).zero;

Alias := Nat;
aliasMain := Alias.succ Alias.zero;
Sequenced := { marker := Unit.unit; Counter Unit; };
blockMain := Sequenced.succ Sequenced.zero;

Indexed := \A : @ => @\n : Nat => {mk : (k : Nat) -> A -> * k;};
Family := Indexed Unit;
indexedMain := Family.mk Nat.zero Unit.unit;
indexedExpected := (Indexed Unit).mk Nat.zero Unit.unit;
