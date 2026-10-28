import pytest

pytest.register_assert_rewrite(
    "support.protocol", "support.snapshot", "support.workspace"
)
