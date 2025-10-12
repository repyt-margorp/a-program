(* Level 0: Simple function types *)

(* Identity function *)
id : * -> * := \A : * => A;

(* Constant function *)
const : * -> * -> * := \A : * => \B : * => A;

(* Function composition type *)
compose_type : (* -> *) -> (* -> *) -> (* -> *) :=
  \F : (* -> *) => \G : (* -> *) => \A : * => F (G A);

main := id;
