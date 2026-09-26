Bool := @{false:*; true:*;};
Nat := @{zero:*; succ:*->*;};
Precedes := @\left:Bool => @\right:Bool => {
	falseBeforeTrue : * Bool.false Bool.true;
};

heightStep := \b:Bool => \ih:(y:Bool)->Precedes y b->Nat => b
	@false => Nat.zero
	@true => Nat.succ (ih Bool.false Precedes.falseBeforeTrue);

zeroStep := \y:Bool => \edge:Precedes y Bool.true => Nat.zero;
oneStep := \y:Bool => \edge:Precedes y Bool.true => Nat.succ Nat.zero;
emptyStep := \y:Bool => \edge:Precedes y Bool.false => Nat.succ Nat.zero;
main := heightStep Bool.true &zeroStep;
second := heightStep Bool.true &oneStep;
base := heightStep Bool.false &emptyStep;
zero := Nat.zero;
one := Nat.succ Nat.zero;
two := Nat.succ one;

Carrier := \b:Bool => b @false => Nat @true => Bool;
extract := \b:Bool => \v:Carrier b => b @false => v @true => Nat.zero;
dependentMain := extract Bool.false two;
dependentBase := extract Bool.true Bool.true;

Box := @\b:Bool => {off:Nat->* Bool.false; on:Bool->* Bool.true;};
consume := \b:Bool => \box:Box b => \use:Box b->Nat => b
	@false => use (Box.off two)
	@true => use box;
offReader := \box:Box Bool.false => box @off n => n;
onReader := \box:Box Bool.true => box @on value => one;
multipleMain := consume Bool.false (Box.off zero) &offReader;
multipleBase := consume Bool.true (Box.on Bool.true) &onReader;
