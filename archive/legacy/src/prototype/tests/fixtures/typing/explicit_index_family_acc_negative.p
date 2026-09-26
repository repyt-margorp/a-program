BadAcc :=
	\A : @ =>
	@\subject : A =>
	{
		bad : ((* subject -> A) -> A) -> * subject;
	};

main := BadAcc;
