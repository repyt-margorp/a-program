Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
Indexed := @\n:Nat => {
	zero : * Nat.zero;
	succ : (k:Nat) -> * (Nat.succ k);
};
use := \n:Nat => \f:Witness n -> Nat => \v:Indexed n =>
	v @zero => f (Witness.at Nat.zero)
	  @succ k => f (Witness.at (Nat.succ k));
use :: (n:Nat) -> (Witness n -> Nat) -> Indexed n -> Nat;
read := \w:Witness (Nat.succ Nat.zero) => w @at k => k;
main := use (Nat.succ Nat.zero) &read (Indexed.succ Nat.zero);
expected := Nat.succ Nat.zero;

AnyIndex := @\n:Nat => {at:(k:Nat)->* k;};
useOriginalName := \n:Nat => \f:Witness n -> Nat => \v:AnyIndex n =>
	v @at k => f (Witness.at n);
useOriginalName :: (n:Nat) -> (Witness n -> Nat) -> AnyIndex n -> Nat;
originalMain := useOriginalName expected &read (AnyIndex.at expected);

useDependentInputs := \n:Nat => \f:Witness n -> Nat => \w:Witness n => \v:Indexed n =>
	v @zero => f w
	  @succ k => f w;
dependentMain := useDependentInputs expected &read (Witness.at expected) (Indexed.succ Nat.zero);
zeroReader := \w:Witness Nat.zero => Nat.succ (Nat.succ Nat.zero);
zeroMain := use Nat.zero &zeroReader Indexed.zero;
zeroExpected := Nat.succ expected;

Point := @\A:@ => @\x:A => {at:(B:@)->(y:B)->* B y;};
usePoint := \A:@ => \x:A => \f:Point A x -> Nat => \v:Point A x =>
	v @at B y => f (Point.at B y);
usePoint :: (A:@) -> (x:A) -> (Point A x -> Nat) -> Point A x -> Nat;
pointReader := \v:Point Nat expected => zeroExpected;
pointMain := usePoint Nat expected &pointReader (Point.at Nat expected);
