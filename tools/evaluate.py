from __future__ import annotations

import argparse
import json
from collections.abc import Sequence
from pathlib import Path
from typing import TypedDict

from sklearn.ensemble import IsolationForest  # type: ignore[import-untyped]

from tools.synth import (
    TRACE_KINDS,
    Trace,
    TraceKind,
    detect_rules,
    features,
    feature_vector,
    generate_traces,
)

TRAINING_SEED = 20261025
EVALUATION_SEED = 20261026
MODEL_RANDOM_STATE = 0
ISOLATION_FOREST_TREES = 100
TRAINING_KINDS: tuple[TraceKind, ...] = ("legit", "legit_hard")
RULE_NAMES = ("speed", "teleport", "aim")


class ConfusionMetrics(TypedDict):
    tp: int
    fp: int
    fn: int
    tn: int
    precision: float
    recall: float
    false_positive_rate: float
    false_negative_rate: float


def metrics(y_true: Sequence[bool], y_predicted: Sequence[bool]) -> ConfusionMetrics:
    if len(y_true) != len(y_predicted):
        raise ValueError("truth and prediction lengths must match")
    if not y_true:
        raise ValueError("metrics require at least one example")

    tp = sum(actual and predicted for actual, predicted in zip(y_true, y_predicted))
    fp = sum(not actual and predicted for actual, predicted in zip(y_true, y_predicted))
    fn = sum(actual and not predicted for actual, predicted in zip(y_true, y_predicted))
    tn = sum(not actual and not predicted for actual, predicted in zip(y_true, y_predicted))
    positive_count = tp + fn
    negative_count = tn + fp
    predicted_positive_count = tp + fp

    return {
        "tp": tp,
        "fp": fp,
        "fn": fn,
        "tn": tn,
        "precision": tp / predicted_positive_count
        if predicted_positive_count
        else 0.0,
        "recall": tp / positive_count if positive_count else 0.0,
        "false_positive_rate": fp / negative_count if negative_count else 0.0,
        "false_negative_rate": fn / positive_count if positive_count else 0.0,
    }


def _trace_features(traces: Sequence[Trace]) -> list[tuple[float, ...]]:
    return [feature_vector(features(trace["steps"])) for trace in traces]


def evaluate(
    training_seed: int = TRAINING_SEED,
    evaluation_seed: int = EVALUATION_SEED,
    traces_per_kind: int = 40,
) -> dict[str, object]:
    if training_seed == evaluation_seed:
        raise ValueError("training and evaluation seeds must differ")
    if traces_per_kind < 1:
        raise ValueError("traces_per_kind must be positive")

    training_traces = generate_traces(
        training_seed, TRAINING_KINDS, traces_per_kind
    )
    evaluation_traces = generate_traces(
        evaluation_seed, TRACE_KINDS, traces_per_kind
    )
    training_vectors = _trace_features(training_traces)
    evaluation_vectors = _trace_features(evaluation_traces)

    model = IsolationForest(
        n_estimators=ISOLATION_FOREST_TREES,
        contamination="auto",
        random_state=MODEL_RANDOM_STATE,
        n_jobs=1,
    )
    model.fit(training_vectors)
    model_predictions = [
        bool(prediction == -1) for prediction in model.predict(evaluation_vectors)
    ]

    evaluation_labels = [trace["kind"] for trace in evaluation_traces]
    truth = [kind not in TRAINING_KINDS for kind in evaluation_labels]
    rule_findings = [
        detect_rules(features(trace["steps"])) for trace in evaluation_traces
    ]
    rule_predictions = [bool(findings) for findings in rule_findings]

    rules_by_name = {
        name: sum(name in findings for findings in rule_findings)
        for name in RULE_NAMES
    }
    rule_counts_by_kind = {
        kind: sum(
            predicted
            for label, predicted in zip(evaluation_labels, rule_predictions)
            if label == kind
        )
        for kind in TRACE_KINDS
    }
    isolation_forest_counts_by_kind = {
        kind: sum(
            predicted
            for label, predicted in zip(evaluation_labels, model_predictions)
            if label == kind
        )
        for kind in TRACE_KINDS
    }

    return {
        "data_kind": "synthetic",
        "training_seed": training_seed,
        "evaluation_seed": evaluation_seed,
        "training_trace_count": len(training_traces),
        "evaluation_trace_count": len(evaluation_traces),
        "training_kinds": list(TRAINING_KINDS),
        "evaluation_kinds": list(TRACE_KINDS),
        "traces_per_kind": traces_per_kind,
        "rule_thresholds": {
            "maximum_speed": 5.75,
            "teleport_distance": 10.0,
            "aim_snap_radians": 3.0,
        },
        "rules": {
            "metrics": metrics(truth, rule_predictions),
            "detections_by_rule": rules_by_name,
            "detected_traces_by_kind": rule_counts_by_kind,
        },
        "isolation_forest": {
            "training_data": "legit and legit_hard traces only",
            "estimators": ISOLATION_FOREST_TREES,
            "contamination": "auto",
            "model_random_state": MODEL_RANDOM_STATE,
            "metrics": metrics(truth, model_predictions),
            "detected_traces_by_kind": isolation_forest_counts_by_kind,
        },
        "evaluation_policy": (
            "Rule thresholds were fixed before evaluation. The Isolation Forest "
            "was fit only on training-seed legitimate traces. Evaluation-seed "
            "traces were used only for reporting."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Evaluate synthetic movement rules and Isolation Forest."
    )
    parser.add_argument("--training-seed", type=int, default=TRAINING_SEED)
    parser.add_argument("--evaluation-seed", type=int, default=EVALUATION_SEED)
    parser.add_argument("--traces-per-kind", type=int, default=40)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    report = evaluate(
        training_seed=args.training_seed,
        evaluation_seed=args.evaluation_seed,
        traces_per_kind=args.traces_per_kind,
    )
    output = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        print(output, end="")
    else:
        args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
