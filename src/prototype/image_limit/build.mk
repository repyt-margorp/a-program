IMAGE_LIMIT_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-image-limit
include $(IMAGE_LIMIT_PROTOTYPE)../conversion_head/build.mk

.PHONY: check-image-limit check-large-image-limit check-finite-sorting check-sorting-backends
check-acceptance: check-image-limit
check-image-limit: $(BUILD)/pointer-check
	bash $(IMAGE_LIMIT_PROTOTYPE)check.sh $(BUILD)/pointer-check

check-large-image-limit: $(BUILD)/pointer-check
	bash $(IMAGE_LIMIT_PROTOTYPE)large-check.sh $(BUILD)/pointer-check

check-finite-sorting: $(BUILD)/pointer-check $(BUILD)/finite_sort_image_compare
	SORTING_IMAGE_LIMIT=3000000 bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/check.sh $(BUILD)/pointer-check $(BUILD)/finite_sort_image_compare all

check-sorting-backends: check-finite-sorting check-image-limit check-large-image-limit $(BUILD)/program_test
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/generic-interfaces-check.sh $(BUILD)/pointer-check
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/value-check.sh $(BUILD)/pointer-check
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/merge-check.sh $(BUILD)/pointer-check $(BUILD)/program_test views
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/merge-generic-check.sh $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/tree-check.sh $(BUILD)/pointer-check $(BUILD)/finite_sort_image_compare
	bash $(IMAGE_LIMIT_PROTOTYPE)../finite_sorting/bubble-check.sh $(BUILD)/pointer-check $(BUILD)/finite_sort_image_compare
	bash $(IMAGE_LIMIT_PROTOTYPE)../conversion_head/source-check.sh $(BUILD)/pointer-check
