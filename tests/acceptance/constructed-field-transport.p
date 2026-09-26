Nat := @{zero:*; succ:*->*;};
At := @\n:Nat => {at:(n:Nat)->* n;};
Consumer := @\n:Nat => {
	use:(k:Nat)->(At k->Nat)->* (Nat.succ k);
};
zero := Nat.zero;
one := Nat.succ zero;
read := \value:At zero => value @at n => n;
consumer := Consumer.use zero &read;
main := consumer @use k f => f (At.at zero);
expected := zero;
readOne := \value:At one => value @at n => n;
secondConsumer := Consumer.use one &readOne;
second := secondConsumer @use k f => f (At.at one);
