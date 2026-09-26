Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->Nat->* k->* (Nat.succ k);};
onlyNil := \xs:Vec Nat.zero => xs @nil => Nat.zero @cons k head tail => Nat.zero;
// This graph producer cannot abstract a fixed index as a generic parameter.
graph := @onlyNil;
