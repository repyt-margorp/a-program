Bool := @{false:*; true:*;};
Nat := @{zero:*; succ:*->*;};
Choice := @{none:*; some:Bool->*; pair:Bool->Nat->*;};
Result := \c:Choice => c @none => Nat @some b => Nat @pair b n => Bool;
choose := \c:Choice => c
	@none => Nat.zero
	@some b => Nat.succ Nat.zero
	@pair b n => b;
choose :: (c:Choice)->Result c;
main := choose (Choice.some Bool.true);
second := choose (Choice.pair Bool.true Nat.zero);
base := choose Choice.none;
zero := Nat.zero;
one := Nat.succ zero;
trueValue := Bool.true;
