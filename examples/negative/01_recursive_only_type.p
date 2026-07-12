/* This must fail.
 * A type with a recursive field but no seed constructor is not accepted as a
 * structural inductive type in the current prototype. There is no base case
 * from which structural recursion can synthesize an IH result classifier.
 */

BadNat := @{
	step : * -> *;
};
