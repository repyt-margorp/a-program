/* This must fail.
 * Recursive occurrences are currently allowed only as direct SELF fields.
 * A field such as * -> * puts SELF under an arrow and is not part of the
 * accepted structural-recursion fragment.
 */

Bad := @{
	base : *;
	wrap : (* -> *) -> *;
};
