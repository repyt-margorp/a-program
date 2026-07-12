/* Draft: append associativity with propositional equality.
 * This file documents the intended A-Program surface form.
 * It is not expected to parse until ==, refl, and cong are implemented.
 */

List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

append := \A : @ =>
	\xs : List A =>
		xs @nil		=> (\ys : List A => ys)
		   @cons x rest	=> (\ys : List A => (List A).cons x (*rest ys));

/*
 * Proposed theorem type.
 *
 * The == form intentionally omits the carrier type. The checker should infer
 * the carrier from both sides:
 *
 *   append A (append A xs ys) zs : List A
 *   append A xs (append A ys zs) : List A
 *
 * then elaborate:
 *
 *   lhs == rhs
 *
 * to:
 *
 *   Eq (List A) lhs rhs
 */
appendAssoc ::
	(A : @) ->
	(xs : List A) ->
	(ys : List A) ->
	(zs : List A) ->
	append A (append A xs ys) zs == append A xs (append A ys zs);

appendAssoc := \A : @ =>
	\xs : List A =>
		xs
			@nil => (
				\ys : List A =>
				\zs : List A =>
					refl (append A ys zs))
			@cons x rest => (
				\ys : List A =>
				\zs : List A =>
					cong
						(\tail : List A => (List A).cons x tail)
						(*rest ys zs));

main := appendAssoc;
