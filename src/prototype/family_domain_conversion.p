// Known rejection at ebe0648; diagnostic only, not an acceptance target.
// Removing the final definition leaves a checked ordinary application.
Nat := @{zero:*;succ:*->*;};
pred := \n:Nat => n @zero => Nat.zero @succ k => k;
Box := @\n:Nat => {mk:(k:Nat)->* k;};
Family := @\n:Nat => @\b:Box n => {mk:(b:Box n)->* n b;};
shift := \k:Nat => \b:Box (pred k) => b;
ordinary := \k:Nat => \b:Box k => (\x:Box k=>x) (shift (Nat.succ k) b);
ordinary :: (k:Nat)->Box k->Box k;
family := \k:Nat => \b:Box k => Family k (shift (Nat.succ k) b);
