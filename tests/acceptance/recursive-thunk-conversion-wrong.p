// Compare suspended functions structurally before normalizing their bodies.
Nat := @{zero:*; succ:*->*;};
Bool := @{yes:*; no:*;};
always_yes := \n:Nat => n @zero => Bool.yes @succ k => *k;
always_no := \n:Nat => Bool.no;
Tag := \f:Nat->Bool => @{mk:*;};
wrong := (Tag (&always_yes)).mk;
wrong :: Tag (&always_no);
