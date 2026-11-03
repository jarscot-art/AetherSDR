# HF5A Windows hang diagnostics

Source: 58f663f7bcda25c54e8f4c158929a6e18ce10011

| Phase | Test | Actual result | Seconds incl. capture |
|---|---|---|---|
| feature-original | backend_family_switch_test | TIMEOUT | 60.31 |
| feature-original | icom_backend_seam_test | TIMEOUT | 60.32 |
| feature-original | icom_control_profile_test | TIMEOUT | 60.4 |
| feature-original | transmit_model_test | TIMEOUT | 60.33 |
| feature-prepared | backend_family_switch_test | PASS | 3.55 |
| feature-prepared | icom_backend_seam_test | PASS | 0.19 |
| feature-prepared | icom_control_profile_test | PASS | 0.29 |
| feature-prepared | transmit_model_test | PASS | 0.11 |

icom_civ_test was built and its DLL imports audited; its previous PASS is not repeated.
A 60-second execution limit is followed by a separate dump/stack capture budget of at most 90 seconds before termination.
Baseline is conditional on a timeout in the prepared feature runtime; missing cases are NOT RUN, never PASS.
