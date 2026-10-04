/* Generated actual successor down branch sequencing into manual C42 actions. */
static const struct qs_acc *gs_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_capture *capture=context; const struct qs_acc *result=NULL; if (!enter(a)) return NULL;
	struct gd_refinement refinement;
	if (!capture || !index_check(a,capture->proof && capture->parameter==capture->proof->subject)
		|| !gd_refine(a,capture->proof,y,edge,&refinement)) goto done;
	switch (edge->tag) {
	case QS_LT_STEP: {
		++a->trace.raw_down_step;
		struct gd_quoted_acc v0;
		if (!gd_transport_right(a,&refinement,capture->proof,&v0)) goto done;
		const struct qs_acc *v1 = gd_force_acc(a,&v0);
		if (!v1 || a->status) goto done;
		result=v1; break;
	}
	case QS_LT_WEAKEN_RIGHT: {
		++a->trace.raw_down_weaken;
		const struct qs_lt *v3 = gd_transport_left(a,&refinement,edge->fields.weaken_right.prior);
		if (!v3) goto done;
		const struct qs_acc *v2 = call_raw_down(a,capture->proof,edge->fields.weaken_right.m,v3);
		if (!v2 || a->status) goto done;
		result=v2; break;
	}
	case QS_LT_LIFT: {
		++a->trace.raw_down_lift;
		const struct qs_lt *v5 = gd_transport_left(a,&refinement,edge->fields.lift.prior);
		if (!v5) goto done;
		const struct qs_acc *v4 = gd_force_ih(a,&capture->ih,edge->fields.lift.m,v5);
		if (!v4 || a->status) goto done;
		result=v4; break;
	}
	default: a->status=2;
	}
done:
	--a->depth; return result;
}
