Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

successor := \value : Nat => Nat.succ value;
successor :: Nat -> Nat;

mapDirect := \xs : List Nat =>
	xs @nil => (List Nat).nil
	   @cons head tail => (List Nat).cons (Nat.succ head) *tail;
mapDirect :: List Nat -> List Nat;

mapHelper := \xs : List Nat =>
	xs @nil => (List Nat).nil
	   @cons head tail => (List Nat).cons (successor head) *tail;
mapHelper :: List Nat -> List Nat;

mapNat := \transform : Nat -> Nat => \xs : List Nat =>
	xs @nil => (List Nat).nil
	   @cons head tail => {
		mappedHead := transform head;
		mappedTail := *tail;
		result := (List Nat).cons mappedHead mappedTail;
	};
mapNat :: (Nat -> Nat) -> List Nat -> List Nat;

map := \A : @ => \B : @ => \transform : A -> B => \xs : List A =>
	xs @nil => (List B).nil
	   @cons head tail => (List B).cons (transform head) *tail;
map :: (A : @) -> (B : @) -> (A -> B) -> List A -> List B;

mapSequenced := \A : @ => \B : @ => \transform : A -> B =>
	\xs : List A =>
		xs @nil => (List B).nil
		   @cons head tail => {
			mappedHead := transform head;
			mappedTail := *tail;
			result := (List B).cons mappedHead mappedTail;
		};

empty := (List Nat).nil;
single := (List Nat).cons Nat.zero (List Nat).nil;
multi := (List Nat).cons Nat.zero
	((List Nat).cons (Nat.succ Nat.zero) (List Nat).nil);

expectedEmpty := {
	result := (List Nat).nil;
};
expectedSingle := {
	result := (List Nat).cons (Nat.succ Nat.zero) (List Nat).nil;
};
expectedMulti := {
	result := (List Nat).cons (Nat.succ Nat.zero)
		((List Nat).cons (Nat.succ (Nat.succ Nat.zero)) (List Nat).nil);
};

directMain := mapDirect multi;
helperMain := mapHelper multi;
monomorphicEmpty := mapNat &successor empty;
monomorphicSingle := mapNat &successor single;
monomorphicMulti := mapNat &successor multi;
polymorphicMain := map Nat Nat &successor multi;
sequencedMain := mapSequenced Nat Nat &successor multi;

main := polymorphicMain;
