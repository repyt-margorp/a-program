Nat := @{
	zero : *;
	succ : * -> *;
};

Tree := @{
	leaf : Nat -> *;
	fork : * -> * -> *;
};

ChoiceTree := @{
	tip : Nat -> *;
	left : * -> *;
	right : * -> *;
};

leftDirect := \tree : Tree =>
	tree @leaf value => value
		@fork left right => *left;

rightDirect := \tree : Tree =>
	tree @leaf value => value
		@fork left right => *right;

bothBlock := \tree : Tree =>
	tree @leaf value => value
		@fork left right => {
			leftValue := *left;
			rightValue := *right;
			leftValue;
		};

leftDirect :: Tree -> Nat;
rightDirect :: Tree -> Nat;
bothBlock :: Tree -> Nat;

add := \left : Nat =>
	left @zero => (\right : Nat => right)
		@succ predecessor =>
			(\right : Nat => Nat.succ (*predecessor right));
add :: Nat -> Nat -> Nat;

size := \tree : Tree =>
	tree @leaf value => Nat.succ Nat.zero
		@fork left right => add *left *right;
size :: Tree -> Nat;

choiceValue := \tree : ChoiceTree =>
	tree @tip value => value
		@left child => *child
		@right child => *child;
choiceValue :: ChoiceTree -> Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := Tree.fork (Tree.leaf one) (Tree.leaf two);

leftResult := leftDirect sample;
rightResult := rightDirect sample;
bothResult := bothBlock sample;
sizeResult := size sample;
choiceLeftResult := choiceValue (ChoiceTree.left (ChoiceTree.tip one));
choiceRightResult := choiceValue (ChoiceTree.right (ChoiceTree.tip two));
leftExpected := { one; };
rightExpected := { two; };
sizeExpected := { two; };
