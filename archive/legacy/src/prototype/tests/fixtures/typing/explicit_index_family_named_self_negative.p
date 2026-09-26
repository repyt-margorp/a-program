List :=
	\A : @ =>
	@{
		nil : List A;
		cons : A -> List A -> List A;
	};

main := List;
