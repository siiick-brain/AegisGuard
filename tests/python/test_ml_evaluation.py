from __future__ import annotations

import json
import math
import random

from hypothesis import given, settings
from hypothesis import strategies as st
import pytest

from tools.evaluate import evaluate, metrics
from tools.synth import (
    TRACE_KINDS,
    detect_rules,
    features,
    generate_traces,
    make_trace,
    TraceKind,
)

LEGITIMATE_KINDS: tuple[TraceKind, ...] = ("legit", "legit_hard")
CHEAT_CASES: tuple[tuple[TraceKind, str], ...] = (
    ("speed", "speed"),
    ("teleport", "teleport"),
    ("aim", "aim"),
)


@settings(max_examples=200, derandomize=True)
@given(
    st.lists(st.tuples(st.booleans(), st.booleans()), min_size=1, max_size=200)
)
def test_confusion_metrics_are_bounded_and_account_for_every_example(
    pairs: list[tuple[bool, bool]],
) -> None:
    truth = [actual for actual, _ in pairs]
    predicted = [guess for _, guess in pairs]

    result = metrics(truth, predicted)

    assert result["tp"] + result["fp"] + result["fn"] + result["tn"] == len(pairs)
    assert 0.0 <= result["precision"] <= 1.0
    assert 0.0 <= result["recall"] <= 1.0
    assert 0.0 <= result["false_positive_rate"] <= 1.0
    assert 0.0 <= result["false_negative_rate"] <= 1.0


@settings(max_examples=100, derandomize=True)
@given(
    st.sampled_from(LEGITIMATE_KINDS),
    st.integers(min_value=0, max_value=10_000),
)
def test_legitimate_traces_do_not_trigger_any_rule(
    kind: TraceKind,
    seed: int,
) -> None:
    trace = make_trace(kind, random.Random(seed))

    assert detect_rules(features(trace["steps"])) == ()


@pytest.mark.parametrize(
    ("kind", "expected_rule"),
    CHEAT_CASES,
)
def test_each_constructed_cheat_triggers_its_named_rule(
    kind: TraceKind,
    expected_rule: str,
) -> None:
    trace = make_trace(kind, random.Random(42))

    assert expected_rule in detect_rules(features(trace["steps"]))


def test_feature_values_are_finite_and_nonnegative() -> None:
    for kind in TRACE_KINDS:
        trace = make_trace(kind, random.Random(123))

        result = features(trace["steps"])

        assert all(math.isfinite(value) and value >= 0.0 for value in result.values())


def test_trace_generation_is_seeded_and_seed_specific() -> None:
    first = generate_traces(81, TRACE_KINDS, 4)
    repeated = generate_traces(81, TRACE_KINDS, 4)
    separate = generate_traces(82, TRACE_KINDS, 4)

    assert first == repeated
    assert first != separate


def test_evaluation_uses_distinct_seeds_and_reports_synthetic_data() -> None:
    report = evaluate(training_seed=91, evaluation_seed=92, traces_per_kind=8)

    assert report["data_kind"] == "synthetic"
    assert report["training_seed"] == 91
    assert report["evaluation_seed"] == 92
    assert report["training_trace_count"] == 16
    assert report["evaluation_trace_count"] == 40
    assert report["training_kinds"] == ["legit", "legit_hard"]
    assert report["isolation_forest"]["training_data"] == (
        "legit and legit_hard traces only"
    )
    json.dumps(report)


def test_evaluation_rejects_training_seed_reuse() -> None:
    with pytest.raises(ValueError, match="seeds must differ"):
        evaluate(training_seed=12, evaluation_seed=12, traces_per_kind=1)


def test_metrics_reject_empty_or_mismatched_inputs() -> None:
    with pytest.raises(ValueError, match="at least one"):
        metrics([], [])
    with pytest.raises(ValueError, match="lengths must match"):
        metrics([True], [])
