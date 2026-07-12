/* Level 2: Simple inductive type - Binary trees */

Nat := @{
  zero : *;
  succ : * -> *;
};

Tree := \A : @ => @{
  leaf : A -> *;
  node : * -> A -> * -> *;
};

/* Size with induction hypothesis */
size : (A : @) -> Tree A -> Nat := \A : @ => \t : Tree A =>
  t @leaf a => Nat.succ Nat.zero
    @node l x r => Nat.succ (add (*l) (*r));

add : Nat -> Nat -> Nat := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat => Nat.succ (*k m));

/* Height with induction hypothesis */
height : (A : @) -> Tree A -> Nat := \A : @ => \t : Tree A =>
  t @leaf a => Nat.zero
    @node l x r => Nat.succ (max (*l) (*r));

max : Nat -> Nat -> Nat := \n : Nat => \m : Nat =>
  n @zero => m
    @succ k => (m @zero => n
                  @succ j => Nat.succ (*k j));

example_tree : Tree Nat :=
  (Tree Nat).node
    ((Tree Nat).leaf Nat.zero)
    Nat.zero
    ((Tree Nat).leaf Nat.zero);

main := size Nat example_tree;
