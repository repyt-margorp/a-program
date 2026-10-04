/* Hand-authored action candidate for the actual three accessibleSucc branches.
	* Only LT-constructor index refinements over the closed Nat representation are
	* represented. It is not generic Identity, source erasure or generated success.
	* Retain the matched edge, two branch paths, map cardinality and original value
	* inside every transported proof/callback. Equal integers alone cannot build it. */
struct gd_nat_path { uint32_t left, right; const struct qs_lt *center; };
struct gd_refinement {
	enum qs_lt_tag branch;
	const struct qs_acc *captured;
	const struct qs_lt *edge;
	struct gd_nat_path paths[2];
	size_t map_count;
};
struct gd_lt_action { struct qs_lt value; struct gd_refinement refinement; const struct qs_lt *original; };
struct gd_acc_action { struct gd_refinement refinement; const struct qs_acc *original; };
struct gd_quoted_acc { struct gd_refinement refinement; const struct qs_acc *original; };
struct gd_folded_ih { struct qs_acc_down original; uint32_t parent; };
struct gd_capture { uint32_t parameter; const struct qs_acc *proof; struct gd_folded_ih ih; };

static const struct qs_acc *gd_accessible_succ(struct qs_arena *, uint32_t, const struct qs_acc *);

static int gd_refine(struct qs_arena *a, const struct qs_acc *proof, uint32_t y,
	const struct qs_lt *edge, struct gd_refinement *out)
{
	if (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation
		&& proof->subject==proof->current && edge && edge->left==y && edge->right==succ(a,proof->current))) return 0;
	uint32_t left,right;
	switch (edge->tag) {
	case QS_LT_STEP: left=edge->fields.step; right=succ(a,left); break;
	case QS_LT_WEAKEN_RIGHT: left=edge->fields.weaken_right.m; right=succ(a,edge->fields.weaken_right.n); break;
	case QS_LT_LIFT: left=succ(a,edge->fields.lift.m); right=succ(a,edge->fields.lift.n); break;
	default: a->status=2; return 0;
	}
	if (!index_check(a,edge->left==left && edge->right==right)) return 0;
	*out=(struct gd_refinement){edge->tag,proof,edge,{{y,left,edge},{edge->right,right,edge}},edge->tag==QS_LT_STEP ? 6 : 10};
	return 1;
}
static int gd_path_valid(struct qs_arena *a, const struct gd_refinement *r)
{
	/* This token is privately constructed by the nominal LT branch above;
		* these checks retain that association, not a source equality decision. */
	return index_check(a,r && r->captured && r->edge && r->captured->domain==&nat_type && r->captured->relation==&lt_relation
		&& r->branch==r->edge->tag && r->map_count==(r->branch==QS_LT_STEP ? 6u : 10u)
		&& r->paths[0].center==r->edge && r->paths[1].center==r->edge
		&& r->paths[0].left==r->edge->left && r->paths[1].left==r->edge->right);
}
static const struct qs_lt *gd_transport_left(struct qs_arena *a, const struct gd_refinement *r, const struct qs_lt *prior)
{
	if (!gd_path_valid(a,r) || !index_check(a,r->branch!=QS_LT_STEP && prior
		&& prior->left==(r->branch==QS_LT_LIFT ? r->edge->fields.lift.m : r->edge->fields.weaken_right.m)
		&& prior->right==(r->branch==QS_LT_LIFT ? r->edge->fields.lift.n : r->edge->fields.weaken_right.n))) return NULL;
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*prior,*r,prior};
	/* Act into the captured callback domain, preserving constructor fields,
		* witness/refinement and original nominal LT value. No pointer erasure. */
	acted->value.right=r->captured->current;
	return &acted->value;
}
static const struct qs_acc *gd_action_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_acc_action *action=context;
	if (!action || !gd_path_valid(a,&action->refinement) || !index_check(a,action->original && edge
		&& edge->left==y && edge->right==action->refinement.paths[0].left)) return NULL;
	/* The Acc field is contravariant in its LT argument: carry the same
		* retained path action into the original nominal callback domain. */
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*edge,action->refinement,edge}; acted->value.right=action->original->current;
	return call_raw_down(a,action->original,y,&acted->value);
}
static int gd_transport_right(struct qs_arena *a, const struct gd_refinement *r,
	const struct qs_acc *proof, struct gd_quoted_acc *out)
{
	if (!gd_path_valid(a,r) || !index_check(a,r->branch==QS_LT_STEP && proof==r->captured
		&& proof->subject==r->edge->fields.step && proof->current==r->edge->fields.step)) return 0;
	/* Source transports a quoted Acc before FORCE; constructing this record
		* does not call the original down or a recursive IH. */
	*out=(struct gd_quoted_acc){*r,proof}; return 1;
}
static const struct qs_acc *gd_force_acc(struct qs_arena *a, const struct gd_quoted_acc *quoted)
{
	if (!quoted || !gd_path_valid(a,&quoted->refinement) || !quoted->original) return NULL;
	struct gd_acc_action *action=allocate(a,sizeof(*action)); struct qs_acc *result=allocate(a,sizeof(*result));
	if (!action || !result) return NULL;
	*action=(struct gd_acc_action){quoted->refinement,quoted->original};
	*result=(struct qs_acc){&nat_type,&lt_relation,quoted->refinement.paths[0].left,
		quoted->refinement.paths[0].left,{gd_action_down,action}};
	return result;
}
static const struct qs_acc *gd_force_ih(struct qs_arena *a, const struct gd_folded_ih *ih, uint32_t y, const struct qs_lt *edge)
{
	if (!index_check(a,ih && ih->original.call && edge && edge->left==y && edge->right==ih->parent)) return NULL;
	const struct qs_acc *child=ih->original.call(a,ih->original.context,y,edge);
	return child && !a->status ? gd_accessible_succ(a,y,child) : NULL;
}
static const struct qs_acc *gd_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_capture *capture=context; const struct qs_acc *result=NULL; if (!enter(a)) return NULL;
	struct gd_refinement refinement;
	if (!capture || !index_check(a,capture->proof && capture->parameter==capture->proof->subject)
		|| !gd_refine(a,capture->proof,y,edge,&refinement)) goto done;
	switch (edge->tag) {
	case QS_LT_STEP: {
		++a->trace.raw_down_step; struct gd_quoted_acc quoted;
		if (gd_transport_right(a,&refinement,capture->proof,&quoted)) result=gd_force_acc(a,&quoted);
		break;
	}
	case QS_LT_WEAKEN_RIGHT: {
		++a->trace.raw_down_weaken;
		const struct qs_lt *prior=gd_transport_left(a,&refinement,edge->fields.weaken_right.prior);
		if (prior) result=call_raw_down(a,capture->proof,edge->fields.weaken_right.m,prior);
		break;
	}
	case QS_LT_LIFT: {
		++a->trace.raw_down_lift;
		const struct qs_lt *prior=gd_transport_left(a,&refinement,edge->fields.lift.prior);
		if (prior) result=gd_force_ih(a,&capture->ih,edge->fields.lift.m,prior);
		break;
	}
	default: a->status=2;
	}
done:
	--a->depth; return result;
}
static const struct qs_acc *gd_accessible_succ(struct qs_arena *a, uint32_t n, const struct qs_acc *proof)
{
	if (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation && proof->subject==n && proof->current==n)) return NULL;
	uint32_t next=succ(a,proof->current); if (a->status) return NULL;
	struct gd_capture *capture=allocate(a,sizeof(*capture)); struct qs_acc *result=allocate(a,sizeof(*result));
	if (!capture || !result) return NULL;
	*capture=(struct gd_capture){n,proof,{proof->down,proof->current}};
	*result=(struct qs_acc){&nat_type,&lt_relation,next,next,{gd_down,capture}}; return result;
}
