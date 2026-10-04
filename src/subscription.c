#include "subscription.h"

#include <stddef.h>

void pg_subscription_detach(struct pg_subscription *edge)
{
	if (!edge || !edge->previous) return;
	*edge->previous = edge->next;
	if (edge->next) edge->next->previous = edge->previous;
	edge->next = NULL;
	edge->previous = NULL;
}

void pg_subscription_attach(struct pg_subscription **list, struct pg_subscription *edge)
{
	pg_subscription_detach(edge);
	edge->next = *list;
	edge->previous = list;
	if (*list) (*list)->previous = &edge->next;
	*list = edge;
}

void pg_subscription_notify(struct pg_subscription **list)
{
	while (*list) {
		struct pg_subscription *edge = *list;
		pg_subscription_detach(edge);
		edge->notify(edge);
	}
}
