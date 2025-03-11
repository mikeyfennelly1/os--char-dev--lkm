#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "../../src/job/job.h"

void test_job(void) {
    CU_ASSERT_EQUAL(job(), 1);
}

int main(void)
{
    // init CUnit test registry
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return CU_get_error();
    }

    // create test suite
    CU_pSuite suite = CU_add_suite("Test suite for test flow", NULL, NULL);
    if (!suite)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (!CU_add_test(suite, "test_job", test_job))
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return 0;
}
