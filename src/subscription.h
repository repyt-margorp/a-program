#ifndef A_PROGRAM_SUBSCRIPTION_H
#define A_PROGRAM_SUBSCRIPTION_H

/* One disposable dependency edge, owned by its subscriber. No input, result,
 * progress or acceptance is stored here. Detach before either owner dies. */
struct pg_subscription {
	struct pg_subscription *next, **previous;
	void (*notify)(struct pg_subscription *);
};

void pg_subscription_attach(struct pg_subscription **list, struct pg_subscription *edge);
void pg_subscription_detach(struct pg_subscription *edge);
void pg_subscription_notify(struct pg_subscription **list);

#endif
