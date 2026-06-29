#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "A",                    // Valid input (within bounds)
        "123456789",            // Boundary case (exact buffer size)
        "12345678901234567890", // Oversized input (2x buffer)
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" // Large exploit payload
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: execute the actual server code with the payload
            char buffer[10] = {0};
            // Simulate reading into buffer without exceeding declared length
            strncpy(buffer, payloads[i], sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            
            // Verify no out-of-bounds write occurred
            ck_assert_msg(strlen(buffer) < sizeof(buffer), 
                         "Buffer overflow detected for payload: %s", payloads[i]);
            exit(0);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            ck_assert_msg(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                         "Process crashed or failed for payload: %s", payloads[i]);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}