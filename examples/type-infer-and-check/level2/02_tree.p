/* Level 2: Simple inductive type - Binary trees */

Nat := @{
  zero : *;
  succ : * -> *;
};

Tree := \A : @ => @{
  leaf : A -> *;
  node : * -> A -> * -> *;
};

add := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat => Nat.succ (*k m));
add :: Nat -> Nat -> Nat;

max := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat =>
      (m @zero => n
         @succ j => Nat.succ (*k j)));
max :: Nat -> Nat -> Nat;

/* Size with induction hypothesis */
size := \A : @ => \t : Tree A =>
  t @leaf a => Nat.succ Nat.zero
    @node l x r => Nat.succ (add (*l) (*r));
size :: (A : @) -> Tree A -> Nat;

/* Height with induction hypothesis */
height := \A : @ => \t : Tree A =>
  t @leaf a => Nat.zero
    @node l x r => Nat.succ (max (*l) (*r));
height :: (A : @) -> Tree A -> Nat;

example_tree := (Tree Nat).node
    ((Tree Nat).leaf Nat.zero)
    Nat.zero
    ((Tree Nat).leaf Nat.zero);
example_tree :: Tree Nat;

main := size Nat example_tree;
