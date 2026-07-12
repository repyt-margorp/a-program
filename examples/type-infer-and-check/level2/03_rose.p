/* Level 2: Complex inductive type - Rose trees (nested recursion) */

Nat := @{
  zero : *;
  succ : * -> *;
};

List := \A : @ => @{
  nil : *;
  cons : A -> * -> *;
};

/* Rose tree: node with list of children */
RoseTree := \A : @ => @{
  rose : A -> List (RoseTree A) -> *;
};

/* Note: This demonstrates nested recursion
   The constructor takes List (RoseTree A), which means
   RoseTree appears nested inside List */

/* Example construction */
empty_children : List (RoseTree Nat) := (List (RoseTree Nat)).nil;

leaf : Nat -> RoseTree Nat := \x : Nat =>
  (RoseTree Nat).rose x empty_children;

example_rose : RoseTree Nat :=
  (RoseTree Nat).rose Nat.zero
    ((List (RoseTree Nat)).cons
      (leaf Nat.zero)
      empty_children);

main := example_rose;
