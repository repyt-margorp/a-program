// LT.step n proves n < succ n, not the n < n required for this descent.
wrong := \access:Acc Nat LT Nat.zero => access
	@acc n down => down n (LT.step n);
