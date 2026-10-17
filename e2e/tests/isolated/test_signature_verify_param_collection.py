"""Isolated regression test for signature certificate fixture collection."""

import ast
from pathlib import Path

import pytest

SIGNATURE_TEST_PATH = (
    Path(__file__).resolve().parents[1] / "basic" / "test_signature_verify.py"
)


def _invalid_certificate_params():
    module = ast.parse(SIGNATURE_TEST_PATH.read_text())
    assignment = next(
        node
        for node in module.body
        if isinstance(node, ast.Assign)
        and any(
            isinstance(target, ast.Name) and target.id == "INVALID_CERT_CASES"
            for target in node.targets
        )
    )
    assert isinstance(assignment.value, ast.List)

    parameters = []
    for entry in assignment.value.elts:
        assert isinstance(entry, ast.Call)
        assert isinstance(entry.func, ast.Attribute) and entry.func.attr == "param"
        assert len(entry.args) == 1
        case = ast.literal_eval(entry.args[0])
        assert isinstance(case, tuple)
        case_id = next(
            ast.literal_eval(keyword.value)
            for keyword in entry.keywords
            if keyword.arg == "id"
        )
        parameters.append(pytest.param(case, id=case_id))
    return parameters


@pytest.fixture(params=_invalid_certificate_params())
def invalid_certificate_case(
    request: pytest.FixtureRequest,
) -> tuple[str, str | None, str]:
    return request.param


def test_invalid_certificate_fixture_parameter_is_one_tuple(
    invalid_certificate_case: tuple[str, str | None, str],
) -> None:
    assert len(invalid_certificate_case) == 3
