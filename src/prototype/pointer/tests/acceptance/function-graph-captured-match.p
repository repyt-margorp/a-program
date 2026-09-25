// A helper may inspect a captured graph parameter without refining its scope.
Nat := @{zero:*; succ:*->*;};
Bool := @{yes:*; no:*;};
Box := @{mk:Nat->*;};
unbox := \box:Box => box @mk value => value;
choose := \b:Bool => \box:Box => b
	@yes => unbox box
	@no => Nat.succ (unbox box);
unbox_graph := \box:Box => box @(self => @unbox self (unbox self))
	@mk value => (@unbox).case0 value;
choose_graph := \b:Bool => \box:Box => b @(self => @choose self box (choose self box))
	@yes => (@choose Bool.yes box (unbox box)).case0 (unbox box) (unbox_graph box)
	@no => (@choose Bool.no box (Nat.succ (unbox box))).case1 (unbox box) (unbox_graph box);
choose_graph :: (b:Bool)->(box:Box)->@choose b box (choose b box);
read_box := \box:Box => \n:Nat => \g:@unbox box n => g @case0 value => value;
read_choice := \b:Bool => \box:Box => \n:Nat => \g:@choose b box n => g
	@case0 value trace => read_box box value trace
	@case1 value trace => Nat.succ (read_box box value trace);
read_choice :: (b:Bool)->(box:Box)->(n:Nat)->@choose b box n->Nat;
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := read_choice Bool.yes (Box.mk one) (choose Bool.yes (Box.mk one)) (choose_graph Bool.yes (Box.mk one));
other := read_choice Bool.no (Box.mk one) (choose Bool.no (Box.mk one)) (choose_graph Bool.no (Box.mk one));
expected := one;
