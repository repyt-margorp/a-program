Nat := @{zero:*; succ:*->*;};
Packet := @{returned:Nat->Nat->*;};
bad := Packet.returned Nat.zero Nat.zero @ output => output;
