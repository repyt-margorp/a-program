Nat := @{zero:*; succ:*->*;};
Bool := @{yes:*; no:*;};
Box := @{mk:Nat->*;};
unbox := \box:Box => box @mk value => value;
choose := \b:Bool => \box:Box => b
	@yes => unbox box
	@no => Nat.succ (unbox box);
wrong := \b:Bool => \box:Box => \n:Nat => \g:@choose b box n => g;
wrong :: (b:Bool)->(box:Box)->(n:Nat)->@choose b box n->@choose b box (Nat.succ n);
