Nat := @{
	zero : *;
	succ : * -> *;
};

Matrix :=
	\A : @ =>
	@\rows : Nat =>
	@\columns : Nat =>
	{
		matrix : (r : Nat) -> (c : Nat) -> A -> * r c;
	};

matrixZero := (Matrix Nat).matrix Nat.zero Nat.zero Nat.zero;
matrixZero :: Matrix Nat Nat.zero Nat.zero;

matrixElement := \A : @ => \rows : Nat => \columns : Nat =>
	\value : Matrix A rows columns =>
		value @matrix r c element => element;
matrixElement :: (A : @) -> (rows : Nat) -> (columns : Nat) ->
	Matrix A rows columns -> A;
matrixZeroElement := matrixElement Nat Nat.zero Nat.zero matrixZero;

DependentIndex :=
	@\A : @ =>
	@\element : A =>
	{
		at : (B : @) -> (value : B) -> * B value;
	};

dependentZero := DependentIndex.at Nat Nat.zero;
dependentZero :: DependentIndex Nat Nat.zero;

Diagonal :=
	\A : @ =>
	@\left : A =>
	@\right : A =>
	{
		same : (value : A) -> * value value;
	};

sameZero := (Diagonal Nat).same Nat.zero;
sameZero :: Diagonal Nat Nat.zero Nat.zero;
