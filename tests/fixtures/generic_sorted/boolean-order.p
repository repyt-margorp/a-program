bool_order := @\left:Bool => @\right:Bool => {
	bottom:(b:Bool)->* Bool.false b;
	top:* Bool.true Bool.true;
};
bool_le := \x:Bool => x
	@false => (\y:Bool => Bool.true)
	@true => (\y:Bool => y);
bool_refl := \x:Bool => x
	@false => bool_order.bottom Bool.false
	@true => bool_order.top;
bool_trans := \x:Bool => \y:Bool => \p:bool_order x y => p
	@bottom b => (\z:Bool => \q:bool_order b z => bool_order.bottom z)
	@top => (\z:Bool => \q:bool_order Bool.true z => q);
bool_trans :: (x:Bool)->(y:Bool)->bool_order x y->(z:Bool)->bool_order y z->bool_order x z;
bool_decide := \x:Bool => \y:Bool => x
	@(self => general_decision Bool &bool_order self y (bool_le self y))
	@false => (general_decision Bool &bool_order Bool.false y).yes (bool_order.bottom y)
	@true => (y @(self => general_decision Bool &bool_order Bool.true self (bool_le Bool.true self))
		@false => (general_decision Bool &bool_order Bool.true Bool.false).no (bool_order.bottom Bool.true)
		@true => (general_decision Bool &bool_order Bool.true Bool.true).yes bool_order.top);
bool_decide :: (x:Bool)->(y:Bool)->general_decision Bool &bool_order x y (bool_le x y);
