/* 0.	path type
 *	\i : [] -> T i
 *	[x = y] : @
 */

/* 1.	when motive is not dependent of x.
 *	transport_{A}^{0->1}(x) := x
 *	motive : A
 */

/* 2.	when motive is a path
 *	tranport_{\s => ([u s = v s] :: (\i : [] => C s i))}^{0->1}(p)
 *		:= \i : [] => transport_{\s => C s i}^{0->1}(p i)
 *	motive :
 *		\s => ([u s = v s] :: (\i : [] => C s i))
 */
