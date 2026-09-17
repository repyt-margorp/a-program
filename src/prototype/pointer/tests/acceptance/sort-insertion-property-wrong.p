import Nat;
import List;
import insertNat;
import Sorted;
import insert_sorted;
// The result index cannot be changed to the unchanged input list.
insert_sorted :: (v:Nat)->(xs:List Nat)->(ys:List Nat)->@insertNat v xs ys->Sorted xs->Sorted xs;
