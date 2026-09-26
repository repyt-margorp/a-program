#ifndef A_PROGRAM_PROTOTYPE_CORE_REQUEST_PROVIDER_H
#define A_PROGRAM_PROTOTYPE_CORE_REQUEST_PROVIDER_H

#include "a_program/core/inspect.h"
#include "a_program/core/request.h"

struct prototype_core_pipeline;

/* Driver-owned provider. Layer T includes request.h only and therefore cannot
 * inspect or recover the mutable CorePipeline behind this capability. */
struct prototype_core_request_service {
	void* provider;
	const void* provider_environment;
	int (*normalize)(
		void* provider,
		const void* provider_environment,
		const struct prototype_core_normalization_request* request,
		struct prototype_core_normalization_response* response
	);
	int (*project_structural_return)(
		void* provider,
		const struct prototype_core_structural_return_projection_request* request,
		struct prototype_core_structural_return_projection_response* response
	);
	int (*inspect)(
		const void* provider,
		const struct prototype_core_inspect_request* request,
		struct prototype_core_inspect_response* response
	);
	int (*form)(
		void* provider,
		const struct prototype_core_formation_request* request,
		const struct prototype_core_request_payload* payload,
		size_t payload_size,
		struct prototype_core_formation_response* response
	);
	int (*rewrite)(
		void* provider,
		const struct prototype_core_rewrite_request* request,
		const struct prototype_core_request_payload* payload,
		size_t payload_size,
		struct prototype_core_rewrite_response* response
	);
};

int prototype_core_request_normalize(
	const struct prototype_core_request_service* service,
	const struct prototype_core_normalization_request* request,
	struct prototype_core_normalization_response* response
);

int prototype_core_request_project_structural_return(
	const struct prototype_core_request_service* service,
	const struct prototype_core_structural_return_projection_request* request,
	struct prototype_core_structural_return_projection_response* response
);

int prototype_core_request_inspect(
	const struct prototype_core_request_service* service,
	const struct prototype_core_inspect_request* request,
	struct prototype_core_inspect_response* response
);

int prototype_core_request_form(
	const struct prototype_core_request_service* service,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* response
);

int prototype_core_request_rewrite(
	const struct prototype_core_request_service* service,
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_rewrite_response* response
);

/* Driver/Core ownership API. Layer T modules must not include this header. */
void prototype_core_request_service_init(
	struct prototype_core_request_service* service,
	struct prototype_core_pipeline* pipeline
);

#endif
