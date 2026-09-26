Nat := @{zero:*; succ:*->*;};
Packet := @{returned:Nat->Nat->*;};
first := \left:Nat => \right:Nat => left;
consume := \left:Nat => \right:Nat => \output:Nat => \graph:@first left right output => output;
valid := Packet.returned Nat.zero Nat.zero @returned output graph => output;
bad := Packet.returned Nat.zero Nat.zero @returned output graph => consume Nat.zero Nat.zero output graph;
