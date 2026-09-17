Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
pick := \b:Bool => b @true => (\n:Nat => n) @false => (\n:Nat => Nat.zero);
Box := @\n:Nat => {mk:(k:Nat)->* k;};
Trace := \f:Bool->Nat->Nat => \b:Bool => \n:Nat => @\result:Nat => {done:* (f b n);};
value := \b:Bool => \n:Nat => Box.mk (pick b n);
consume := \p:Box (Nat.succ Nat.zero) => Nat.zero;
// The path supplies zero, not an arbitrary replacement for the computed index.
wrong := \b:Bool => \n:Nat => \trace:Trace (&pick) b n Nat.zero => trace @done => consume (value b n);
