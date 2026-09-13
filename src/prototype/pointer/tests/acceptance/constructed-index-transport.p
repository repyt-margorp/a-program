Nat := @{zero:*; succ:*->*;};
At := @\n:Nat => {at:(n:Nat)->* n;};
Consumer := @\n:Nat => {use:(n:Nat)->(At n->Nat)->* n;};
one := Nat.succ Nat.zero;
two := Nat.succ one;
read := \value:At one => value @at n => n;
consumer := Consumer.use one &read;
main := consumer @use n f => f (At.at one);
expected := one;
