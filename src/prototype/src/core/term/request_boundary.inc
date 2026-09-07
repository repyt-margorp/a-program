#include "a_program/core/request.h"
#include "a_program/core/request_provider.h"
#include "a_program/core/pipeline.h"

static int core_request_normalize(
	void* provider,
	const void* provider_environment,
	const struct prototype_core_normalization_request* request,
	struct prototype_core_normalization_response* response
) {
	if (!provider || !request || !response) {
		return -1;
	}
	(void)provider_environment;
	return prototype_core_pipeline_normalize(provider, request, response);
}

static int core_request_project_structural_return(
	void* provider,
	const struct prototype_core_structural_return_projection_request* request,
	struct prototype_core_structural_return_projection_response* response
) {
	return provider && request && response ?
		prototype_core_pipeline_project_structural_return(
			provider, request, response
		) : -1;
}

static int core_request_inspect(
	const void* provider,
	const struct prototype_core_inspect_request* request,
	struct prototype_core_inspect_response* response
) {
	return provider && request && response ?
		prototype_core_pipeline_inspect(provider, request, response) : -1;
}

static int core_request_form(
	void* provider,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* response
) {
	return provider && request && response ?
		prototype_core_pipeline_form(
			provider, request, payload, payload_size, response
		) : -1;
}

static int core_request_rewrite(
	void* provider,
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_rewrite_response* response
) {
	return provider && request && response ?
		prototype_core_pipeline_rewrite(
			provider, request, payload, payload_size, response
		) : -1;
}

int prototype_core_request_normalize(
	const struct prototype_core_request_service* service,
	const struct prototype_core_normalization_request* request,
	struct prototype_core_normalization_response* response
) {
	if (!service || !service->provider || !service->normalize ||
		!request || !response) {
		return -1;
	}
	return service->normalize(
		service->provider,
		service->provider_environment,
		request,
		response
	);
}

int prototype_core_request_project_structural_return(
	const struct prototype_core_request_service* service,
	const struct prototype_core_structural_return_projection_request* request,
	struct prototype_core_structural_return_projection_response* response
) {
	if (!service || !service->provider ||
		!service->project_structural_return || !request || !response) {
		return -1;
	}
	return service->project_structural_return(
		service->provider, request, response
	);
}

int prototype_core_request_inspect(
	const struct prototype_core_request_service* service,
	const struct prototype_core_inspect_request* request,
	struct prototype_core_inspect_response* response
) {
	if (!service || !service->provider || !service->inspect ||
		!request || !response) {
		return -1;
	}
	return service->inspect(service->provider, request, response);
}

int prototype_core_request_form(
	const struct prototype_core_request_service* service,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* response
) {
	if (!service || !service->provider || !service->form ||
		!request || !response) {
		return -1;
	}
	return service->form(
		service->provider, request, payload, payload_size, response
	);
}

int prototype_core_request_rewrite(
	const struct prototype_core_request_service* service,
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_rewrite_response* response
) {
	if (!service || !service->provider || !service->rewrite ||
		!request || !response) {
		return -1;
	}
	return service->rewrite(
		service->provider, request, payload, payload_size, response
	);
}

void prototype_core_request_service_init(
	struct prototype_core_request_service* service,
	struct prototype_core_pipeline* pipeline
) {
	if (!service || !pipeline) {
		return;
	}
	service->provider = pipeline;
	service->provider_environment = NULL;
	service->normalize = core_request_normalize;
	service->project_structural_return = core_request_project_structural_return;
	service->inspect = core_request_inspect;
	service->form = core_request_form;
	service->rewrite = core_request_rewrite;
}
