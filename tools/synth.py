from __future__ import annotations

import math
import random
from collections.abc import Sequence
from typing import Literal, TypedDict

TraceKind = Literal["legit", "legit_hard", "speed", "teleport", "aim"]
Step = tuple[float, float, float]

STEPS_PER_SECOND = 60.0
DEFAULT_STEP_COUNT = 60


class Trace(TypedDict):
    kind: TraceKind
    steps: list[Step]


class MovementFeatures(TypedDict):
    maximum_speed: float
    maximum_step_distance: float
    maximum_aim_delta: float


class RuleThresholds(TypedDict):
    maximum_speed: float
    teleport_distance: float
    aim_snap_radians: float


TRACE_KINDS: tuple[TraceKind, ...] = (
    "legit",
    "legit_hard",
    "speed",
    "teleport",
    "aim",
)
RULE_THRESHOLDS: RuleThresholds = {
    "maximum_speed": 5.75,
    "teleport_distance": 10.0,
    "aim_snap_radians": 3.0,
}


def make_trace(
    kind: TraceKind,
    rng: random.Random,
    step_count: int = DEFAULT_STEP_COUNT,
) -> Trace:
    if step_count < 2:
        raise ValueError("step_count must be at least 2")

    x = 0.0
    y = 0.0
    heading = 0.0
    aim = 0.0
    steps: list[Step] = [(x, y, aim)]

    for index in range(1, step_count):
        if kind == "legit":
            heading += rng.uniform(-0.35, 0.35)
            speed = rng.uniform(0.5, 4.5)
            aim += rng.uniform(-0.04, 0.04)
        elif kind == "legit_hard":
            heading += rng.uniform(-math.pi, math.pi)
            speed = rng.uniform(2.0, 5.5)
            aim += rng.uniform(-0.08, 0.08)
        elif kind == "speed":
            speed = 8.0
            aim += rng.uniform(-0.04, 0.04)
        else:
            speed = rng.uniform(0.5, 4.5)
            aim += rng.uniform(-0.04, 0.04)

        distance = speed / STEPS_PER_SECOND
        x += math.cos(heading) * distance
        y += math.sin(heading) * distance

        if kind == "teleport" and index == step_count // 2:
            x += 11.0
        if kind == "aim" and index == step_count // 2:
            aim += 3.1

        steps.append((x, y, aim))

    return {"kind": kind, "steps": steps}


def features(steps: Sequence[Step]) -> MovementFeatures:
    if not steps:
        raise ValueError("steps must not be empty")

    maximum_speed = 0.0
    maximum_step_distance = 0.0
    maximum_aim_delta = 0.0

    for step in steps:
        if len(step) != 3 or not all(math.isfinite(value) for value in step):
            raise ValueError("each step must contain finite x, y and aim values")

    for previous, current in zip(steps, steps[1:]):
        dx = current[0] - previous[0]
        dy = current[1] - previous[1]
        distance = math.hypot(dx, dy)
        aim_delta = abs(
            (current[2] - previous[2] + math.pi) % (2.0 * math.pi) - math.pi
        )
        maximum_step_distance = max(maximum_step_distance, distance)
        maximum_speed = max(maximum_speed, distance * STEPS_PER_SECOND)
        maximum_aim_delta = max(maximum_aim_delta, aim_delta)

    return {
        "maximum_speed": maximum_speed,
        "maximum_step_distance": maximum_step_distance,
        "maximum_aim_delta": maximum_aim_delta,
    }


def detect_rules(
    movement_features: MovementFeatures,
    thresholds: RuleThresholds = RULE_THRESHOLDS,
) -> tuple[str, ...]:
    findings: list[str] = []
    step_distance = movement_features["maximum_step_distance"]
    speed = movement_features["maximum_speed"]

    if step_distance > thresholds["teleport_distance"]:
        findings.append("teleport")
    elif speed > thresholds["maximum_speed"]:
        findings.append("speed")

    if movement_features["maximum_aim_delta"] > thresholds["aim_snap_radians"]:
        findings.append("aim")

    return tuple(findings)


def feature_vector(movement_features: MovementFeatures) -> tuple[float, ...]:
    return (
        movement_features["maximum_speed"] / RULE_THRESHOLDS["maximum_speed"],
        movement_features["maximum_step_distance"]
        / RULE_THRESHOLDS["teleport_distance"],
        movement_features["maximum_aim_delta"]
        / RULE_THRESHOLDS["aim_snap_radians"],
    )


def generate_traces(
    seed: int,
    kinds: Sequence[TraceKind],
    traces_per_kind: int,
    step_count: int = DEFAULT_STEP_COUNT,
) -> list[Trace]:
    if traces_per_kind < 1:
        raise ValueError("traces_per_kind must be positive")

    rng = random.Random(seed)
    traces: list[Trace] = []
    for kind in kinds:
        for _ in range(traces_per_kind):
            traces.append(make_trace(kind, rng, step_count))
    return traces
