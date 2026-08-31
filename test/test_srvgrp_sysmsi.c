// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_SYSMSI_COUNT	2
#define TEST_SYSMSI_VALID_INDEX	0
#define TEST_SYSMSI_INVALID_INDEX	TEST_SYSMSI_COUNT
#define TEST_SYSMSI_ADDR_LOW	0x12345000
#define TEST_SYSMSI_ADDR_HIGH	0x0
#define TEST_SYSMSI_DATA	0x55aa55aa
#define TEST_SYSMSI_BAD_ADDR_LOW	0xdead0000
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_attrs_expdata[] = {
	RPMI_SUCCESS,
	TEST_SYSMSI_COUNT,
	0,
	0,
};

static rpmi_uint32_t get_msi_attrs_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
};

static rpmi_uint32_t get_msi_attrs_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSMSI_MSI_ATTRIBUTES_FLAG0_PREF_PRIV,
	0,
	0,
	0,
	0,
	0,
};

static rpmi_uint32_t invalid_msi_reqdata[] = {
	TEST_SYSMSI_INVALID_INDEX,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t set_msi_state_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	RPMI_SYSMSI_MSI_STATE_ENABLE,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_msi_state_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
};

static rpmi_uint32_t get_msi_state_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSMSI_MSI_STATE_ENABLE,
};

static rpmi_uint32_t set_msi_target_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	TEST_SYSMSI_ADDR_LOW,
	TEST_SYSMSI_ADDR_HIGH,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t get_msi_target_expdata[] = {
	RPMI_SUCCESS,
	TEST_SYSMSI_ADDR_LOW,
	TEST_SYSMSI_ADDR_HIGH,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t set_msi_bad_target_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	TEST_SYSMSI_BAD_ADDR_LOW,
	0,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t invalid_addr_expdata[] = {
	RPMI_ERR_INVALID_ADDR,
};

/* Index 1 is a valid MSI but not M-mode preferred: PREF_PRIV must be clear */
#define TEST_SYSMSI_NONPREF_INDEX	1
static rpmi_uint32_t get_msi_attrs_nonpref_reqdata[] = {
	TEST_SYSMSI_NONPREF_INDEX,
};

static rpmi_uint32_t get_msi_attrs_nonpref_expdata[] = {
	RPMI_SUCCESS,
	0, /* mmode_preferred(index 1) == false -> no PREF_PRIV */
	0,
	0,
	0,
	0,
	0,
};

/* SET_MSI_STATE requires 8 bytes; carry a full request with an invalid index */
static rpmi_uint32_t set_msi_state_invalid_reqdata[] = {
	TEST_SYSMSI_INVALID_INDEX,
	RPMI_SYSMSI_MSI_STATE_ENABLE,
};

/* SET_MSI_TARGET requires 16 bytes; carry a full request with an invalid index */
static rpmi_uint32_t set_msi_target_invalid_reqdata[] = {
	TEST_SYSMSI_INVALID_INDEX,
	TEST_SYSMSI_ADDR_LOW,
	TEST_SYSMSI_ADDR_HIGH,
	TEST_SYSMSI_DATA,
};

/* Disable a previously-enabled MSI and read the cleared state back */
static rpmi_uint32_t set_msi_state_disable_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	0,
};

static rpmi_uint32_t get_msi_state_disabled_expdata[] = {
	RPMI_SUCCESS,
	0,
};

static rpmi_bool_t test_validate_msi_addr(void *priv, rpmi_uint64_t msi_addr)
{
	return msi_addr == TEST_SYSMSI_ADDR_LOW;
}

static rpmi_bool_t test_mmode_preferred(void *priv, rpmi_uint32_t msi_index)
{
	return msi_index == TEST_SYSMSI_VALID_INDEX;
}

static struct rpmi_sysmsi_platform_ops test_sysmsi_ops = {
	.validate_msi_addr = test_validate_msi_addr,
	.mmode_preferred = test_mmode_preferred,
};

static int test_sysmsi_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_sysmsi_create(TEST_SYSMSI_COUNT,
						 TEST_SYSMSI_VALID_INDEX,
						 &test_sysmsi_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi sysmsi service group");
		return RPMI_ERR_FAILED;
	}

	ret = rpmi_context_add_group(scene->cntx, grp);
	if (ret) {
		rpmi_service_group_sysmsi_destroy(grp);
		test_scenario_default_cleanup(scene);
	}

	return ret;
}

/*
 * No service requests are repeated here because SYSTEM_MSI handlers are
 * privilege-independent. Successful initialization verifies that the group is
 * admitted to an S-mode context.
 */
static struct rpmi_test_scenario scenario_sysmsi_s_mode_admission = {
	.name = "System MSI Service Group S-mode Admission",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.privilege_level = RPMI_PRIVILEGE_S_MODE,

	.init = test_sysmsi_scenario_init,
	.cleanup = test_scenario_default_cleanup,
	.num_tests = 0,
};

struct test_sysmsi_delivery_config {
	struct rpmi_service_group *grp;
};

static struct rpmi_test_scenario scenario_sysmsi_default = {
	.name = "System MSI Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.privilege_level = RPMI_PRIVILEGE_M_MODE,
	.priv = NULL,

	.init = test_sysmsi_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 16,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata,
				.request_data_len = sizeof(enable_notif_reqdata),
				.expected_data = enable_notif_expdata,
				.expected_data_len = sizeof(enable_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_attrs_expdata,
				.expected_data_len = sizeof(get_attrs_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_attrs_reqdata,
				.request_data_len = sizeof(get_msi_attrs_reqdata),
				.expected_data = get_msi_attrs_expdata,
				.expected_data_len = sizeof(get_msi_attrs_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_msi_reqdata,
				.request_data_len = sizeof(invalid_msi_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI STATE (enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_reqdata,
				.request_data_len = sizeof(set_msi_state_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI STATE (enabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_state_expdata,
				.expected_data_len = sizeof(get_msi_state_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI STATE (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_msi_reqdata,
				.request_data_len = sizeof(invalid_msi_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI TARGET",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_target_reqdata,
				.request_data_len = sizeof(set_msi_target_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI TARGET",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_target_expdata,
				.expected_data_len = sizeof(get_msi_target_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI TARGET (invalid address)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_bad_target_reqdata,
				.request_data_len = sizeof(set_msi_bad_target_reqdata),
				.expected_data = invalid_addr_expdata,
				.expected_data_len = sizeof(invalid_addr_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES (non-preferred index, PREF_PRIV clear)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_attrs_nonpref_reqdata,
				.request_data_len = sizeof(get_msi_attrs_nonpref_reqdata),
				.expected_data = get_msi_attrs_nonpref_expdata,
				.expected_data_len = sizeof(get_msi_attrs_nonpref_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI STATE (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_invalid_reqdata,
				.request_data_len = sizeof(set_msi_state_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI TARGET (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_target_invalid_reqdata,
				.request_data_len = sizeof(set_msi_target_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI TARGET (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_msi_reqdata,
				.request_data_len = sizeof(invalid_msi_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI STATE (disable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_disable_reqdata,
				.request_data_len = sizeof(set_msi_state_disable_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI STATE (disabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_state_disabled_expdata,
				.expected_data_len = sizeof(get_msi_state_disabled_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

/*
 * MSI delivery scenario.
 *
 * The default scenario never triggers MSI delivery because their target
 * address is a fake constant.  Here the target is a real writable buffer whose
 * address passes validate_msi_addr, so injecting an enabled+targeted MSI runs
 * rpmi_sysmsi_process_events -> rpmi_env_writel and we can observe the write.
 * The delivery ops also provide get_name to cover that GET_MSI_ATTRIBUTES
 * branch.
 */
#define TEST_SYSMSI_NAME	"msi0"

static rpmi_uint32_t test_delivery_target;

static rpmi_bool_t test_delivery_validate_msi_addr(void *priv, rpmi_uint64_t msi_addr)
{
	return msi_addr == (rpmi_uint64_t)(rpmi_uintptr_t)&test_delivery_target;
}

static void test_delivery_get_name(void *priv, rpmi_uint32_t msi_index,
				   char *out_name, rpmi_uint32_t out_name_sz)
{
	rpmi_env_strncpy(out_name, TEST_SYSMSI_NAME, out_name_sz);
}

static struct rpmi_sysmsi_platform_ops test_delivery_ops = {
	.validate_msi_addr = test_delivery_validate_msi_addr,
	.mmode_preferred = test_mmode_preferred,
	.get_name = test_delivery_get_name,
};

static struct test_sysmsi_delivery_config sysmsi_delivery_config;

static int test_sysmsi_delivery_init(struct rpmi_test_scenario *scene)
{
	struct test_sysmsi_delivery_config *config;
	struct rpmi_service_group *grp;
	int ret;

	if (!scene || !scene->priv)
		return RPMI_ERR_INVALID_PARAM;
	config = scene->priv;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_sysmsi_create(TEST_SYSMSI_COUNT,
					       TEST_SYSMSI_VALID_INDEX,
					       &test_delivery_ops, NULL);
	if (!grp) {
		printf("%s: failed to create rpmi sysmsi service group\n", __func__);
		return RPMI_ERR_FAILED;
	}

	ret = rpmi_context_add_group(scene->cntx, grp);
	if (ret) {
		printf("%s: failed to add sysmsi group (error %d)\n", __func__, ret);
		rpmi_service_group_sysmsi_destroy(grp);
		return ret;
	}

	config->grp = grp;
	test_delivery_target = 0;
	return 0;
}

/* Build a SET_MSI_TARGET request pointing at the real delivery buffer */
static rpmi_uint16_t test_delivery_set_target_reqdata(struct rpmi_test_scenario *scene,
						      struct rpmi_test *test,
						      void *data, rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *req = data;
	rpmi_uintptr_t addr = (rpmi_uintptr_t)&test_delivery_target;

	req[0] = TEST_SYSMSI_VALID_INDEX;
	req[1] = (rpmi_uint32_t)addr;
	req[2] = (rpmi_uint32_t)((rpmi_uint64_t)addr >> 32);
	req[3] = TEST_SYSMSI_DATA;
	return 4 * sizeof(*req);
}

/* Build the expected GET_MSI_ATTRIBUTES response including the MSI name */
static rpmi_uint16_t test_delivery_msi_attrs_expdata(struct rpmi_test_scenario *scene,
						     struct rpmi_test *test,
						     void *data, rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = data;

	exp[0] = RPMI_SUCCESS;
	exp[1] = RPMI_SYSMSI_MSI_ATTRIBUTES_FLAG0_PREF_PRIV; /* index 0 preferred */
	exp[2] = 0;
	rpmi_env_strncpy((char *)&exp[3], TEST_SYSMSI_NAME, 4 * sizeof(*exp));
	return 7 * sizeof(*exp);
}

static rpmi_uint32_t get_msi_state_pending_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSMSI_MSI_STATE_PENDING,
};

static void test_delivery_noop_wait(struct rpmi_test_scenario *scene,
				    struct rpmi_test *test, struct rpmi_message *msg)
{
	rpmi_env_memset(msg, 0, scene->slot_size);
}

/* Deliver via the P2A doorbell inject path (p2a_msi_index == valid index) */
static int test_delivery_inject_p2a_run(struct rpmi_test_scenario *scene,
					struct rpmi_test *test, struct rpmi_message *msg)
{
	struct test_sysmsi_delivery_config *config = scene->priv;

	return rpmi_service_group_sysmsi_inject_p2a(config->grp);
}

static int test_delivery_verify_delivered(struct rpmi_test_scenario *scene,
					  struct rpmi_test *test, struct rpmi_message *msg)
{
	if (test_delivery_target != TEST_SYSMSI_DATA) {
		printf("%s: MSI not delivered: target=0x%x expected=0x%x\n",
		       test->name, test_delivery_target, TEST_SYSMSI_DATA);
		return 1;
	}
	return 0;
}

/* Inject while disabled: process_events must NOT write the target */
static int test_delivery_inject_disabled_run(struct rpmi_test_scenario *scene,
					     struct rpmi_test *test, struct rpmi_message *msg)
{
	struct test_sysmsi_delivery_config *config = scene->priv;

	test_delivery_target = 0;
	return rpmi_service_group_sysmsi_inject(config->grp, TEST_SYSMSI_VALID_INDEX);
}

static int test_delivery_verify_not_delivered(struct rpmi_test_scenario *scene,
					      struct rpmi_test *test, struct rpmi_message *msg)
{
	if (test_delivery_target != 0) {
		printf("%s: MSI delivered while disabled: target=0x%x\n",
		       test->name, test_delivery_target);
		return 1;
	}
	return 0;
}

/* Injecting an out-of-range index must return INVALID_PARAM */
static int test_delivery_inject_invalid_run(struct rpmi_test_scenario *scene,
					    struct rpmi_test *test, struct rpmi_message *msg)
{
	struct test_sysmsi_delivery_config *config = scene->priv;
	enum rpmi_error rc;

	rc = rpmi_service_group_sysmsi_inject(config->grp, TEST_SYSMSI_INVALID_INDEX);
	if (rc != RPMI_ERR_INVALID_PARAM) {
		printf("%s: inject(invalid) returned %d, expected INVALID_PARAM\n",
		       test->name, rc);
		return RPMI_ERR_FAILED;
	}
	return 0;
}

static struct rpmi_test_scenario scenario_sysmsi_delivery = {
	.name = "System MSI Service Group Delivery",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.privilege_level = RPMI_PRIVILEGE_M_MODE,
	.priv = &sysmsi_delivery_config,

	.init = test_sysmsi_delivery_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 8,
	.tests = {
		{
			.name = "SET MSI TARGET (delivery buffer)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_delivery_set_target_reqdata,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI STATE (enable for delivery)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_reqdata,
				.request_data_len = sizeof(set_msi_state_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES (with name)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_attrs_reqdata,
				.request_data_len = sizeof(get_msi_attrs_reqdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_delivery_msi_attrs_expdata,
		},
		{
			.name = "INJECT P2A (deliver MSI)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.flags = RPMI_MSG_POSTED_REQUEST,
			},
			.run = test_delivery_inject_p2a_run,
			.wait = test_delivery_noop_wait,
			.verify = test_delivery_verify_delivered,
		},
		{
			.name = "SET MSI STATE (disable before pending test)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_disable_reqdata,
				.request_data_len = sizeof(set_msi_state_disable_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "INJECT while disabled (no delivery)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.flags = RPMI_MSG_POSTED_REQUEST,
			},
			.run = test_delivery_inject_disabled_run,
			.wait = test_delivery_noop_wait,
			.verify = test_delivery_verify_not_delivered,
		},
		{
			.name = "GET MSI STATE (pending, not enabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_state_pending_expdata,
				.expected_data_len = sizeof(get_msi_state_pending_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "INJECT (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.flags = RPMI_MSG_POSTED_REQUEST,
			},
			.run = test_delivery_inject_invalid_run,
			.wait = test_delivery_noop_wait,
		},
	},
};

/*
 * Direct-API checks for the inject entry points that are not reachable through
 * the message path: NULL group guards and inject_p2a when the P2A doorbell MSI
 * index is out of range (NOTSUPP).
 */
static int test_sysmsi_inject_api(void)
{
	struct rpmi_service_group *grp;
	int ret = 0;

	if (rpmi_service_group_sysmsi_inject(NULL, 0) != RPMI_ERR_INVALID_PARAM) {
		printf("%s: inject(NULL) did not return INVALID_PARAM\n", __func__);
		ret = RPMI_ERR_FAILED;
	}
	if (rpmi_service_group_sysmsi_inject_p2a(NULL) != RPMI_ERR_INVALID_PARAM) {
		printf("%s: inject_p2a(NULL) did not return INVALID_PARAM\n", __func__);
		ret = RPMI_ERR_FAILED;
	}

	/* p2a_msi_index >= num_msi is clamped at create; inject_p2a -> NOTSUPP */
	grp = rpmi_service_group_sysmsi_create(TEST_SYSMSI_COUNT, TEST_SYSMSI_COUNT,
					       &test_sysmsi_ops, NULL);
	if (!grp) {
		printf("%s: failed to create group\n", __func__);
		return RPMI_ERR_FAILED;
	}
	if (rpmi_service_group_sysmsi_inject_p2a(grp) != RPMI_ERR_NOTSUPP) {
		printf("%s: inject_p2a with out-of-range P2A index != NOTSUPP\n", __func__);
		ret = RPMI_ERR_FAILED;
	}
	rpmi_service_group_sysmsi_destroy(grp);

	printf("TEST: %-50s \t : %s!\n", "sysmsi inject API",
	       ret ? "Failed" : "Succeeded");
	return ret;
}

int main(int argc, char *argv[])
{
	int rc;

	printf("Test System MSI Service Group\n");

	rc = test_scenario_execute(&scenario_sysmsi_default);
	if (rc)
		return rc;

	rc = test_scenario_execute(&scenario_sysmsi_s_mode_admission);
	if (rc)
		return rc;

	rc = test_scenario_execute(&scenario_sysmsi_delivery);
	if (rc)
		return rc;

	return test_sysmsi_inject_api();
}
