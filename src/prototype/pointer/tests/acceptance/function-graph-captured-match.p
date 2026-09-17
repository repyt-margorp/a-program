// A helper may inspect a captured graph parameter without refining its scope.
Nat := @{zero:*; succ:*->*;};
Bool := @{yes:*; no:*;};
Box := @{mk:Nat->*;};
unbox := \box:Box => box @mk value => value;
choose := \b:Bool => \box:Box => b
	@yes => unbox box
	@no => Nat.succ (unbox box);
read_box := \box:Box => \n:Nat => \g:@unbox box n => g @case0 value => value;
read_choice := \b:Bool => \box:Box => \n:Nat => \g:@choose b box n => g
	@case0 value trace => read_box box value trace
	@case1 value trace => Nat.succ (read_box box value trace);
read_choice :: (b:Bool)->(box:Box)->(n:Nat)->@choose b box n->Nat;
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := *choose Bool.yes (Box.mk one) @output => read_choice Bool.yes (Box.mk one) output @output;
other := *choose Bool.no (Box.mk one) @output => read_choice Bool.no (Box.mk one) output @output;
expected := one;
