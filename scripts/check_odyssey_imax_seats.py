#!/usr/bin/env python3
"""Check Fandango for useful IMAX 70mm seats for The Odyssey.

The default search covers these theaters:

* Universal Cinema AMC at CityWalk Hollywood
* Regal Irvine Spectrum
* Regal Edwards Ontario Palace

It checks August 10, 12, and 14-18, 2026, choosing IMAX 70mm showtimes in the
11:00 AM-noon and 11:00 PM-midnight windows, plus showtimes nearest 3:00 PM
and 7:00 PM.  The morning and late-night windows receive a 10-point final-score
penalty.  At CityWalk and Irvine, an alert is raised when two physically
adjacent seats are available in row 4 or farther back.

Ontario uses a stricter limit: two together in row 5 or farther back.  Each
matching pair receives a 0-100 seat-quality rating that prioritizes
horizontal centering and targets a row slightly behind the seating midpoint.
Results below 70/100 after the final-score penalty are suppressed and do not
alert.

Rows are counted from the screen using their order in Fandango's seat map.
The script only reads showtimes and seat availability.  It never selects seats
or advances toward checkout.  Dates before the local current date are ignored,
including dates supplied with ``--date``.

Setup on Windows (PowerShell):

    python -m pip install playwright

The script uses installed Google Chrome by default.  If Chrome is unavailable,
install Playwright's Chromium and select it explicitly:

    python -m playwright install chromium
    python .\\scripts\\check_odyssey_imax_seats.py --browser-channel chromium

Run once:

    python .\\scripts\\check_odyssey_imax_seats.py

Poll every 15 minutes (new availability beeps once per change):

    python .\\scripts\\check_odyssey_imax_seats.py --repeat-minutes 15

The checker uses Fandango's read-only "Check Seats" preview rather than its
checkout flow and runs Chrome headlessly by default.  Pass ``--headed`` to
watch the browser if Fandango changes the page and troubleshooting is needed.
"""

from __future__ import annotations

import argparse
import asyncio
import re
import statistics
import sys
from dataclasses import dataclass, replace
from datetime import date, datetime, time, timedelta
from typing import Any, Iterable, Sequence
from urllib.parse import parse_qs, urlparse


MOVIE_ID = "241386"

DEFAULT_DATES = (
    date(2026, 8, 10),
    date(2026, 8, 12),
    date(2026, 8, 14),
    date(2026, 8, 15),
    date(2026, 8, 16),
    date(2026, 8, 17),
    date(2026, 8, 18),
)

DEFAULT_TARGET_TIMES = (
    time(11, 30),
    time(15, 0),
    time(19, 0),
    time(23, 30),
)
OFF_HOUR_TARGETS = frozenset((time(11, 30), time(23, 30)))
OFF_HOUR_WINDOW_MINUTES = 30
OFF_HOUR_TIME_PENALTY = 10
EXCELLENT_MIN_SCORE = 85
GOOD_MIN_SCORE = 70
ACCEPTABLE_MIN_SCORE = 55
MINIMUM_SHOWN_RATING = GOOD_MIN_SCORE
# IMAX 1.43:1 guidance favors the middle of the row, a little behind the
# seating midpoint.  These are explicit heuristics so the model is inspectable.
IDEAL_ROW_FRACTION = 0.58
FRONT_ROW_CURVE = 1.5
BACK_ROW_SCORE_FLOOR = 55
ROW_SCORE_WEIGHT = 0.40
CENTER_SCORE_WEIGHT = 0.60
SEAT_PREVIEW_ATTEMPTS = 3
SEAT_DATA_WAIT_MS = 10_000

ANSI_BRIGHT_GREEN = "\033[92m"
ANSI_GREEN = "\033[32m"
ANSI_BRIGHT_CYAN = "\033[96m"
ANSI_BRIGHT_YELLOW = "\033[93m"
ANSI_BRIGHT_RED = "\033[91m"
ANSI_RESET = "\033[0m"


@dataclass(frozen=True)
class Theater:
    key: str
    name: str
    page_url: str
    theater_id: str


THEATERS = {
    "citywalk": Theater(
        key="citywalk",
        name="Universal Cinema AMC at CityWalk Hollywood",
        page_url=(
            "https://www.fandango.com/"
            "universal-cinema-amc-at-citywalk-hollywood-aaawx/theater-page"
        ),
        theater_id="AAAWX",
    ),
    "irvine": Theater(
        key="irvine",
        name="Regal Irvine Spectrum",
        page_url=(
            "https://www.fandango.com/"
            "regal-irvine-spectrum-aabtb/theater-page"
        ),
        theater_id="AABTB",
    ),
    "ontario": Theater(
        key="ontario",
        name="Regal Edwards Ontario Palace",
        page_url=(
            "https://www.fandango.com/"
            "regal-edwards-ontario-palace-screenx-imax-and-rpx-aaedm/"
            "theater-page"
        ),
        theater_id="AAEDM",
    ),
}


SEAT_LABEL_RE = re.compile(
    r"^Seat:\s*(?P<row>[A-Z]+)(?P<number>\d+)\s*-\s*"
    r"This seat is (?P<status>available|unavailable)\.$",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class Showtime:
    theater: Theater
    starts_at: datetime
    target_time: time
    jump_url: str


@dataclass(frozen=True)
class Seat:
    row: str
    number: int
    available: bool
    x: float
    y: float
    width: float
    row_number: int = 0

    @property
    def label(self) -> str:
        return f"{self.row}{self.number}"


@dataclass(frozen=True)
class SeatRun:
    row: str
    row_number: int
    seats: tuple[Seat, ...]
    base_score: int = 0
    quality_score: int = 0
    quality_label: str = "UNRATED"
    row_score: int = 0
    center_score: int = 0

    @property
    def label(self) -> str:
        if len(self.seats) == 1:
            return self.seats[0].label
        numbers = sorted(seat.number for seat in self.seats)
        return f"{self.row}{numbers[0]}-{self.row}{numbers[-1]}"


@dataclass(frozen=True)
class Availability:
    showtime: Showtime
    seat_map_url: str
    pair_runs: tuple[SeatRun, ...]
    schedule_penalty: int = 0

    @property
    def has_alert(self) -> bool:
        return bool(self.pair_runs)


def terminal_color(text: str, ansi_color: str) -> str:
    """Return colored terminal text when stdout is interactive."""
    if not sys.stdout.isatty():
        return text
    return f"{ansi_color}{text}{ANSI_RESET}"


def rating_color(score: int) -> str:
    if score >= EXCELLENT_MIN_SCORE:
        return ANSI_BRIGHT_GREEN
    if score >= GOOD_MIN_SCORE:
        return ANSI_BRIGHT_CYAN
    if score >= ACCEPTABLE_MIN_SCORE:
        return ANSI_BRIGHT_YELLOW
    return ANSI_BRIGHT_RED


def color_by_rating(text: str, score: int) -> str:
    return terminal_color(text, rating_color(score))


def best_rating(result: Availability) -> int:
    return max((run.quality_score for run in result.pair_runs), default=0)


def distinct_alert_runs(result: Availability) -> tuple[SeatRun, ...]:
    """Return each matching two-seat pair once."""
    return result.pair_runs


def print_found(
    result: Availability,
    pair_min_row: int,
) -> None:
    """Print a matching showtime and its booking link as soon as it is read."""
    showtime = result.showtime
    score = best_rating(result)
    print("", flush=True)
    print(
        color_by_rating(
            f"FOUND SEATS: {showtime.theater.name} | "
            f"{showtime.starts_at:%a %Y-%m-%d %I:%M %p}",
            score,
        ),
        flush=True,
    )
    if result.schedule_penalty:
        print(
            terminal_color(
                f"  Off-hour target: -{result.schedule_penalty} points "
                "applied to final score.",
                ANSI_BRIGHT_YELLOW,
            ),
            flush=True,
        )
    if result.pair_runs:
        print(
            f"  2 together, row {pair_min_row}+: "
            f"{format_runs(result.pair_runs)}",
            flush=True,
        )
    print(
        color_by_rating(f"  OPEN NOW: {result.seat_map_url}", score),
        flush=True,
    )
    print("", flush=True)


def parse_date(value: str) -> date:
    try:
        return date.fromisoformat(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"invalid date {value!r}; use YYYY-MM-DD"
        ) from exc


def parse_time(value: str) -> time:
    try:
        return time.fromisoformat(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"invalid time {value!r}; use HH:MM in 24-hour time"
        ) from exc


def dates_not_in_past(
    dates: Iterable[date], today: date | None = None
) -> list[date]:
    """Return unique dates that are today or later in local time."""
    reference_date = (
        today if today is not None else datetime.now().astimezone().date()
    )
    return sorted(
        {target_date for target_date in dates if target_date >= reference_date}
    )


def minutes_between(left: time, right: time) -> int:
    left_minutes = left.hour * 60 + left.minute
    right_minutes = right.hour * 60 + right.minute
    return abs(left_minutes - right_minutes)


def target_has_time_penalty(target: time) -> bool:
    target_minutes = target.hour * 60 + target.minute
    return (
        11 * 60 <= target_minutes <= 12 * 60
        or target_minutes >= 23 * 60
        or target_minutes == 0
    )


def minimum_pair_row(theater: Theater, pair_min_row: int) -> int:
    """Apply Ontario's stricter minimum rows without weakening CLI limits."""
    if theater.key == "ontario":
        return max(pair_min_row, 5)
    return pair_min_row


def parse_showtime_link(
    theater: Theater, target_date: date, href: str
) -> tuple[datetime, str] | None:
    query = parse_qs(urlparse(href).query)
    if query.get("mid", [None])[0] != MOVIE_ID:
        return None
    if (query.get("tid", [""])[0] or "").upper() != theater.theater_id:
        return None

    raw_start = query.get("sdate", [None])[0]
    if raw_start is None:
        return None
    try:
        starts_at = datetime.strptime(raw_start, "%Y-%m-%d+%H:%M")
    except ValueError:
        return None
    if starts_at.date() != target_date:
        return None
    return starts_at, href


def select_target_showtimes(
    theater: Theater,
    target_date: date,
    hrefs: Iterable[str],
    targets: Sequence[time],
    window_minutes: int,
) -> list[Showtime]:
    parsed: dict[datetime, str] = {}
    for href in hrefs:
        result = parse_showtime_link(theater, target_date, href)
        if result is not None:
            parsed[result[0]] = result[1]

    selected: list[Showtime] = []
    used_starts: set[datetime] = set()
    for target in targets:
        candidates = [
            (minutes_between(starts.time(), target), starts, href)
            for starts, href in parsed.items()
            if starts not in used_starts
        ]
        if not candidates:
            continue
        distance, starts_at, href = min(candidates, key=lambda item: item[:2])
        target_window = (
            OFF_HOUR_WINDOW_MINUTES
            if target in OFF_HOUR_TARGETS
            else window_minutes
        )
        if distance > target_window:
            continue
        selected.append(
            Showtime(
                theater=theater,
                starts_at=starts_at,
                target_time=target,
                jump_url=href,
            )
        )
        used_starts.add(starts_at)
    return selected


def parse_seats(raw_seats: Sequence[dict[str, Any]]) -> list[Seat]:
    seats: list[Seat] = []
    row_order: list[str] = []

    for raw in raw_seats:
        label = str(raw.get("label") or "")
        match = SEAT_LABEL_RE.match(label)
        if match is None:
            continue
        row = match.group("row").upper()
        if row.startswith("WC"):
            continue
        if row not in row_order:
            row_order.append(row)
        seats.append(
            Seat(
                row=row,
                number=int(match.group("number")),
                available=match.group("status").lower() == "available",
                x=float(raw.get("x") or 0.0),
                y=float(raw.get("y") or 0.0),
                width=float(raw.get("width") or 0.0),
            )
        )

    row_numbers = {row: index + 1 for index, row in enumerate(row_order)}
    return [replace(seat, row_number=row_numbers[seat.row]) for seat in seats]


def row_adjacency_threshold(row_seats: Sequence[Seat]) -> float:
    ordered = sorted(row_seats, key=lambda seat: seat.x)
    gaps = [
        right.x - left.x
        for left, right in zip(ordered, ordered[1:])
        if right.x - left.x > 0.1
    ]
    widths = [seat.width for seat in ordered if seat.width > 0.1]
    if gaps:
        normal_gap = statistics.median(gaps)
    elif widths:
        normal_gap = statistics.median(widths) * 1.25
    else:
        normal_gap = 20.0
    normal_width = statistics.median(widths) if widths else normal_gap
    return max(normal_gap * 1.65, normal_width * 1.9)


def physically_adjacent(left: Seat, right: Seat, threshold: float) -> bool:
    return (
        left.row == right.row
        and abs(left.number - right.number) == 1
        and abs(left.x - right.x) <= threshold
    )


def rate_seat_run(run: SeatRun, auditorium_seats: Sequence[Seat]) -> SeatRun:
    """Rate view quality from row depth and horizontal centering.

    The ideal row is slightly behind the seating midpoint, while horizontal
    centering carries the larger weight.  The back row remains viable, while
    rows close to the screen lose points quickly.  This is a view-quality score,
    not a ticket-price or resale-value calculation.
    """
    total_rows = max((seat.row_number for seat in auditorium_seats), default=1)
    row_fraction = run.row_number / total_rows
    if row_fraction <= IDEAL_ROW_FRACTION:
        front_progress = row_fraction / IDEAL_ROW_FRACTION
        row_score = 100.0 * front_progress**FRONT_ROW_CURVE
    else:
        back_progress = (row_fraction - IDEAL_ROW_FRACTION) / (
            1.0 - IDEAL_ROW_FRACTION
        )
        row_score = 100.0 - (100 - BACK_ROW_SCORE_FLOOR) * back_progress

    centers = [seat.x + seat.width / 2.0 for seat in auditorium_seats]
    auditorium_left = min(centers, default=0.0)
    auditorium_right = max(centers, default=0.0)
    auditorium_center = (auditorium_left + auditorium_right) / 2.0
    half_span = max((auditorium_right - auditorium_left) / 2.0, 1.0)
    run_center = statistics.mean(
        seat.x + seat.width / 2.0 for seat in run.seats
    )
    center_score = 100.0 * max(
        0.0,
        1.0 - abs(run_center - auditorium_center) / half_span,
    )

    score = round(
        ROW_SCORE_WEIGHT * row_score + CENTER_SCORE_WEIGHT * center_score
    )
    return replace(
        run,
        base_score=score,
        quality_score=score,
        quality_label=quality_label(score),
        row_score=round(row_score),
        center_score=round(center_score),
    )


def quality_label(score: int) -> str:
    if score >= EXCELLENT_MIN_SCORE:
        return "EXCELLENT"
    if score >= GOOD_MIN_SCORE:
        return "GOOD"
    if score >= ACCEPTABLE_MIN_SCORE:
        return "ACCEPTABLE"
    return "WEAK"


def penalize_runs(
    runs: Sequence[SeatRun],
    penalty: int,
) -> tuple[SeatRun, ...]:
    """Apply a schedule penalty while preserving physical seat quality."""
    if penalty <= 0:
        return tuple(runs)
    penalized: list[SeatRun] = []
    for run in runs:
        base_score = run.base_score or run.quality_score
        score = max(0, base_score - penalty)
        penalized.append(
            replace(
                run,
                base_score=base_score,
                quality_score=score,
                quality_label=quality_label(score),
            )
        )
    return tuple(penalized)


def qualifying_rated_runs(runs: Sequence[SeatRun]) -> tuple[SeatRun, ...]:
    """Suppress low-quality seats from alerts and all displayed results."""
    return tuple(
        run for run in runs if run.quality_score >= MINIMUM_SHOWN_RATING
    )


def make_seat_run(
    run_seats: Sequence[Seat],
    auditorium_seats: Sequence[Seat],
) -> SeatRun:
    run = SeatRun(
        row=run_seats[0].row,
        row_number=run_seats[0].row_number,
        seats=tuple(run_seats),
    )
    return rate_seat_run(run, auditorium_seats)


def available_runs(seats: Sequence[Seat], min_row: int) -> list[SeatRun]:
    runs: list[SeatRun] = []
    rows = sorted(
        {seat.row for seat in seats if seat.row_number >= min_row},
        key=lambda row: next(seat.row_number for seat in seats if seat.row == row),
    )
    for row in rows:
        row_seats = [seat for seat in seats if seat.row == row]
        available = sorted(
            (seat for seat in row_seats if seat.available),
            key=lambda seat: seat.x,
        )
        if not available:
            continue
        threshold = row_adjacency_threshold(row_seats)
        current = [available[0]]
        for seat in available[1:]:
            if physically_adjacent(current[-1], seat, threshold):
                current.append(seat)
            else:
                runs.append(make_seat_run(current, seats))
                current = [seat]
        runs.append(make_seat_run(current, seats))
    return runs


def find_pair_runs(seats: Sequence[Seat], min_row: int) -> tuple[SeatRun, ...]:
    """Return every adjacent two-seat option, including pairs in longer runs."""
    pairs: list[SeatRun] = []
    for run in available_runs(seats, min_row):
        for left, right in zip(run.seats, run.seats[1:]):
            pairs.append(make_seat_run((left, right), seats))
    return tuple(pairs)


def format_runs(runs: Sequence[SeatRun], limit: int = 18) -> str:
    labels = [
        color_by_rating(
            f"{run.label} (row {run.row_number}, {run.quality_score}/100 "
            f"{run.quality_label}"
            + (
                f", base {run.base_score}"
                if run.base_score != run.quality_score
                else ""
            )
            + f"; row {run.row_score}, "
            f"center {run.center_score})",
            run.quality_score,
        )
        for run in runs
    ]
    if len(labels) > limit:
        labels = labels[:limit] + [f"... and {len(labels) - limit} more"]
    return ", ".join(labels)


def availability_keys(result: Availability) -> set[str]:
    prefix = (
        f"{result.showtime.theater.key}|"
        f"{result.showtime.starts_at.isoformat()}"
    )
    return {
        f"{prefix}|pair|{run.label}"
        for run in result.pair_runs
    }


def audible_alert() -> None:
    try:
        import winsound

        winsound.MessageBeep(winsound.MB_ICONEXCLAMATION)
    except (ImportError, RuntimeError):
        print("\a", end="", flush=True)


async def discover_showtimes(
    page: Any,
    theater: Theater,
    target_date: date,
    targets: Sequence[time],
    window_minutes: int,
    timeout_ms: int,
) -> list[Showtime]:
    url = f"{theater.page_url}?date={target_date.isoformat()}"
    await page.goto(url, wait_until="domcontentloaded", timeout=timeout_ms)
    movie_link = f'a[href*="tickets.fandango.com"][href*="mid={MOVIE_ID}"]'
    try:
        await page.wait_for_selector(movie_link, timeout=timeout_ms)
    except Exception:
        return []
    hrefs = await page.locator(movie_link).evaluate_all(
        "elements => elements.map(element => element.href)"
    )
    return select_target_showtimes(
        theater=theater,
        target_date=target_date,
        hrefs=hrefs,
        targets=targets,
        window_minutes=window_minutes,
    )


async def read_seat_previews(
    page: Any,
    showtimes: Sequence[Showtime],
    pair_min_row: int,
    timeout_ms: int,
) -> list[Availability]:
    if not showtimes:
        return []

    movie_link = page.locator(
        f'a[href*="tickets.fandango.com"][href*="mid={MOVIE_ID}"]'
    ).first
    amenity_group = movie_link.locator(
        'xpath=ancestor::section[contains(@class, "shared-showtimes__amenity-group")]'
    )
    check_seats = amenity_group.get_by_role("button", name="Check Seats")
    await check_seats.click(force=True, timeout=timeout_ms)

    modal = page.locator("#seat-map-preview-modal.open")
    await modal.wait_for(state="visible", timeout=timeout_ms)
    # Fandango displays the modal shell before its asynchronous seat request
    # necessarily finishes.  Wait for actual standard-seat data, but preserve
    # the detailed diagnostic below if the wait expires.
    try:
        await modal.locator('.smp__seat[data-type="standard"]').first.wait_for(
            state="attached",
            timeout=min(timeout_ms, SEAT_DATA_WAIT_MS),
        )
    except Exception:
        pass
    raw_previews = await modal.locator(".smp__wrap").evaluate_all(
        """
        wraps => wraps.map(wrap => {
            const href = wrap.querySelector("a.smp__buy-tickets-btn")?.href || "";
            return {
                href,
                seats: Array.from(wrap.querySelectorAll(".smp__seat")).map(seat => {
                    const available = seat.classList.contains("smp__seat--A");
                    const id = seat.dataset.id || "";
                    return {
                        label: `Seat: ${id} - This seat is ${available ? "available" : "unavailable"}.`,
                        x: Number(seat.dataset.x || 0),
                        y: Number(seat.dataset.y || 0),
                        width: Number(seat.dataset.w || 0),
                        type: seat.dataset.type || ""
                    };
                })
            };
        })
        """
    )

    previews_by_start: dict[datetime, dict[str, Any]] = {}
    target_date = showtimes[0].starts_at.date()
    theater = showtimes[0].theater
    for preview in raw_previews:
        parsed = parse_showtime_link(theater, target_date, str(preview.get("href") or ""))
        if parsed is not None:
            previews_by_start[parsed[0]] = preview

    results: list[Availability] = []
    for showtime in showtimes:
        preview = previews_by_start.get(showtime.starts_at)
        if preview is None:
            raise RuntimeError(
                f"Fandango preview omitted the {showtime.starts_at:%H:%M} showtime"
            )
        preview_seats = list(preview.get("seats", []))
        raw_seats = [
            seat
            for seat in preview_seats
            if str(seat.get("type") or "").lower() == "standard"
        ]
        if not raw_seats:
            observed_types = sorted(
                {
                    str(seat.get("type") or "<missing>")
                    for seat in preview_seats
                }
            )
            detail = (
                f"{len(preview_seats)} seat elements; types: "
                f"{', '.join(observed_types)}"
                if preview_seats
                else "0 seat elements loaded"
            )
            raise RuntimeError(
                f"Fandango preview returned no standard seats for "
                f"{showtime.starts_at:%H:%M} ({detail})"
            )
        seats = parse_seats(raw_seats)
        if not seats:
            raise RuntimeError(
                f"Fandango preview seat labels changed for "
                f"{showtime.starts_at:%H:%M}"
            )
        schedule_penalty = (
            OFF_HOUR_TIME_PENALTY
            if target_has_time_penalty(showtime.target_time)
            else 0
        )
        pair_runs = qualifying_rated_runs(
            penalize_runs(
                find_pair_runs(seats, pair_min_row),
                schedule_penalty,
            )
        )
        results.append(
            Availability(
                showtime=showtime,
                seat_map_url=showtime.jump_url,
                pair_runs=pair_runs,
                schedule_penalty=schedule_penalty,
            )
        )
    return results


async def check_cycle(
    page: Any,
    theaters: Sequence[Theater],
    dates: Sequence[date],
    targets: Sequence[time],
    pair_min_row: int,
    window_minutes: int,
    timeout_ms: int,
    delay_seconds: float,
) -> tuple[list[Availability], list[str]]:
    results: list[Availability] = []
    errors: list[str] = []
    total_groups = len(theaters) * len(dates)
    group_number = 0

    for theater in theaters:
        theater_pair_min_row = minimum_pair_row(theater, pair_min_row)
        for target_date in dates:
            group_number += 1
            print(
                f"[{group_number:02d}/{total_groups:02d}] {theater.name} | "
                f"{target_date.isoformat()}: loading IMAX 70mm showtimes...",
                flush=True,
            )
            try:
                showtimes = await discover_showtimes(
                    page=page,
                    theater=theater,
                    target_date=target_date,
                    targets=targets,
                    window_minutes=window_minutes,
                    timeout_ms=timeout_ms,
                )
            except Exception as exc:
                errors.append(
                    f"{theater.name} {target_date.isoformat()}: "
                    f"showtime lookup failed: {exc}"
                )
                print(f"  ERROR: showtime lookup failed: {exc}", flush=True)
                await asyncio.sleep(delay_seconds)
                continue

            found_targets = {showtime.target_time for showtime in showtimes}
            for target in targets:
                if target not in found_targets:
                    errors.append(
                        f"{theater.name} {target_date.isoformat()} near "
                        f"{target.strftime('%-I:%M %p') if sys.platform != 'win32' else target.strftime('%I:%M %p').lstrip('0')}: "
                        "no matching IMAX 70mm showtime"
                    )

            if showtimes:
                actual_times = ", ".join(
                    showtime.starts_at.strftime("%I:%M %p").lstrip("0")
                    for showtime in showtimes
                )
                print(
                    f"  Found {actual_times}; reading Fandango seat preview...",
                    flush=True,
                )
                try:
                    date_results: list[Availability] | None = None
                    last_preview_error: Exception | None = None
                    for attempt in range(1, SEAT_PREVIEW_ATTEMPTS + 1):
                        try:
                            date_results = await read_seat_previews(
                                page=page,
                                showtimes=showtimes,
                                pair_min_row=theater_pair_min_row,
                                timeout_ms=timeout_ms,
                            )
                            break
                        except Exception as exc:
                            last_preview_error = exc
                            if attempt == SEAT_PREVIEW_ATTEMPTS:
                                raise
                            retry_delay = float(attempt * 2)
                            print(
                                f"  Seat data was incomplete on attempt "
                                f"{attempt}/{SEAT_PREVIEW_ATTEMPTS}: {exc}",
                                flush=True,
                            )
                            print(
                                f"  Retrying the Fandango page in "
                                f"{retry_delay:g} seconds...",
                                flush=True,
                            )
                            await asyncio.sleep(retry_delay)
                            showtimes = await discover_showtimes(
                                page=page,
                                theater=theater,
                                target_date=target_date,
                                targets=targets,
                                window_minutes=window_minutes,
                                timeout_ms=timeout_ms,
                            )
                            if not showtimes:
                                raise RuntimeError(
                                    "IMAX 70mm showtimes disappeared while "
                                    "retrying the seat preview"
                                ) from last_preview_error
                    if date_results is None:
                        raise RuntimeError(
                            "seat preview retry loop produced no result"
                        ) from last_preview_error
                    results.extend(date_results)
                    matched = sum(result.has_alert for result in date_results)
                    if matched:
                        for result in date_results:
                            if result.has_alert:
                                print_found(
                                    result,
                                    pair_min_row=theater_pair_min_row,
                                )
                        print(
                            terminal_color(
                                f"  FOUND: {matched}/{len(date_results)} "
                                "showtime(s) have qualifying seats.",
                                rating_color(
                                    max(
                                        best_rating(result)
                                        for result in date_results
                                        if result.has_alert
                                    )
                                ),
                            ),
                            flush=True,
                        )
                    else:
                        print(
                            f"  Checked {len(date_results)} showtime(s): "
                            "no qualifying seats.",
                            flush=True,
                        )
                except Exception as exc:
                    errors.append(
                        f"{theater.name} {target_date.isoformat()}: "
                        f"seat preview failed: {exc}"
                    )
                    print(f"  ERROR: seat preview failed: {exc}", flush=True)
            else:
                print(
                    "  No IMAX 70mm showtime found in the requested time windows.",
                    flush=True,
                )
            await asyncio.sleep(delay_seconds)
    return results, errors


def print_cycle(results: Sequence[Availability], errors: Sequence[str]) -> None:
    checked_at = datetime.now().astimezone().strftime("%Y-%m-%d %I:%M:%S %p %Z")
    print(f"\nChecked at {checked_at}")
    print("=" * 78)

    for result in results:
        showtime = result.showtime
        if result.has_alert:
            score = best_rating(result)
            print(
                color_by_rating(
                    f"[FOUND SEATS] {showtime.theater.name} | "
                    f"{showtime.starts_at:%a %Y-%m-%d %I:%M %p}",
                    score,
                )
            )
            if result.schedule_penalty:
                print(
                    terminal_color(
                        f"  Off-hour target: -{result.schedule_penalty} "
                        "points applied to final score.",
                        ANSI_BRIGHT_YELLOW,
                    )
                )
            if result.pair_runs:
                print(
                    f"  2 together: {format_runs(result.pair_runs)}"
                )
            print(
                color_by_rating(f"  OPEN NOW: {result.seat_map_url}", score)
            )
        else:
            print(
                f"[no qualifying seats] {showtime.theater.name} | "
                f"{showtime.starts_at:%a %Y-%m-%d %I:%M %p}"
            )

    if errors:
        print("\nNotes:")
        for error in errors:
            print(f"  - {error}")

    alert_count = sum(result.has_alert for result in results)
    print(
        f"\nSummary: checked {len(results)} seat maps; "
        f"{alert_count} showtimes have qualifying availability."
    )

    found_results = [result for result in results if result.has_alert]
    print("\n" + "=" * 78)
    print("FOUND-SEATS SUMMARY")
    if not found_results:
        print("No qualifying seats found in this scan.")
        return

    for index, result in enumerate(found_results, start=1):
        showtime = result.showtime
        score = best_rating(result)
        print(
            color_by_rating(
                f"{index}. {showtime.theater.name} | "
                f"{showtime.starts_at:%a %Y-%m-%d %I:%M %p} | "
                f"best {score}/100",
                score,
            )
        )
        if result.schedule_penalty:
            print(
                terminal_color(
                    f"   Off-hour target: -{result.schedule_penalty} points "
                    "applied to final score.",
                    ANSI_BRIGHT_YELLOW,
                )
            )
        print(f"   Seats: {format_runs(distinct_alert_runs(result))}")
        print(
            color_by_rating(f"   OPEN NOW: {result.seat_map_url}", score)
        )


def run_self_test() -> None:
    raw = []
    for row_index, row in enumerate(("A", "B", "C", "D", "E")):
        for number in range(1, 7):
            x = number * 10.0 + (25.0 if number >= 4 else 0.0)
            available = row == "D" and number in (1, 2, 3, 4)
            available = available or (row == "E" and number == 5)
            raw.append(
                {
                    "label": (
                        f"Seat: {row}{number} - This seat is "
                        f"{'available' if available else 'unavailable'}."
                    ),
                    "x": x,
                    "y": row_index * 10.0,
                    "width": 8.0,
                }
            )
    seats = parse_seats(raw)
    pairs = find_pair_runs(seats, min_row=4)
    assert [run.label for run in pairs] == ["D1-D2", "D2-D3"]
    assert all(len(run.seats) == 2 for run in pairs)
    assert all(0 <= run.quality_score <= 100 for run in pairs)
    assert all(
        abs(
            run.quality_score
            - (
                ROW_SCORE_WEIGHT * run.row_score
                + CENTER_SCORE_WEIGHT * run.center_score
            )
        )
        <= 1
        for run in pairs
    )
    timed_pairs = penalize_runs(pairs, OFF_HOUR_TIME_PENALTY)
    assert timed_pairs[0].base_score == pairs[0].quality_score
    assert (
        timed_pairs[0].quality_score
        == max(0, pairs[0].quality_score - OFF_HOUR_TIME_PENALTY)
    )
    assert timed_pairs[0].row_score == pairs[0].row_score
    assert timed_pairs[0].center_score == pairs[0].center_score
    assert not qualifying_rated_runs(
        (replace(pairs[0], quality_score=MINIMUM_SHOWN_RATING - 1),)
    )
    assert qualifying_rated_runs(
        (replace(pairs[0], quality_score=MINIMUM_SHOWN_RATING),)
    )
    assert MINIMUM_SHOWN_RATING == GOOD_MIN_SCORE
    assert target_has_time_penalty(time(11, 30))
    assert target_has_time_penalty(time(23, 30))
    assert not target_has_time_penalty(time(15, 0))
    assert not target_has_time_penalty(time(19, 0))
    assert dates_not_in_past(
        (
            date(2026, 8, 10),
            date(2026, 8, 11),
            date(2026, 8, 13),
            date(2026, 8, 13),
        ),
        today=date(2026, 8, 11),
    ) == [date(2026, 8, 11), date(2026, 8, 13)]
    assert quality_label(39) == "WEAK"
    assert quality_label(40) == "WEAK"
    assert quality_label(49) == "WEAK"
    assert quality_label(50) == "WEAK"
    assert quality_label(54) == "WEAK"
    assert quality_label(55) == "ACCEPTABLE"
    assert quality_label(69) == "ACCEPTABLE"
    assert quality_label(70) == "GOOD"
    assert quality_label(84) == "GOOD"
    assert quality_label(85) == "EXCELLENT"
    assert minimum_pair_row(THEATERS["citywalk"], 4) == 4
    assert minimum_pair_row(THEATERS["ontario"], 4) == 5
    print("Self-test passed.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--theater",
        action="append",
        choices=tuple(THEATERS),
        help="theater to check; repeat this option (default: all three)",
    )
    parser.add_argument(
        "--date",
        action="append",
        type=parse_date,
        help=(
            "date to check as YYYY-MM-DD; repeat this option "
            "(past dates are ignored)"
        ),
    )
    parser.add_argument(
        "--target-time",
        action="append",
        type=parse_time,
        help="target time as HH:MM; repeat this option",
    )
    parser.add_argument("--pair-min-row", type=int, default=4)
    parser.add_argument(
        "--window-minutes",
        type=int,
        default=60,
        help="maximum difference from each target time (default: 60)",
    )
    parser.add_argument(
        "--delay-seconds",
        type=float,
        default=1.5,
        help="pause between Fandango page loads (default: 1.5)",
    )
    parser.add_argument(
        "--repeat-minutes",
        type=float,
        default=0.0,
        help="repeat after this many minutes; 0 checks once (minimum: 5)",
    )
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument(
        "--browser-channel",
        choices=("chrome", "msedge", "chromium"),
        default="chrome",
        help="browser executable to use (default: installed Chrome)",
    )
    parser.add_argument("--self-test", action="store_true")
    return parser


async def async_main(args: argparse.Namespace) -> int:
    try:
        from playwright.async_api import async_playwright
    except ImportError:
        print(
            "Playwright is required. Install it with:\n"
            "  python -m pip install playwright\n"
            "  python -m playwright install chromium",
            file=sys.stderr,
        )
        return 2

    if args.repeat_minutes and args.repeat_minutes < 5:
        print("--repeat-minutes must be 0 or at least 5", file=sys.stderr)
        return 2
    if args.pair_min_row < 1:
        print("minimum row numbers must be positive", file=sys.stderr)
        return 2

    theaters = [
        THEATERS[key]
        for key in (args.theater or tuple(THEATERS))
    ]
    requested_dates = sorted(set(args.date or DEFAULT_DATES))
    dates = dates_not_in_past(requested_dates)
    skipped_dates = [
        target_date for target_date in requested_dates if target_date not in dates
    ]
    if skipped_dates:
        print(
            "Skipping past date(s): "
            + ", ".join(target_date.isoformat() for target_date in skipped_dates),
            flush=True,
        )
    if not dates:
        print("No current or future dates remain to check.", flush=True)
        return 0
    targets = sorted(set(args.target_time or DEFAULT_TARGET_TIMES))
    timeout_ms = int(args.timeout_seconds * 1000)
    previous_keys: set[str] = set()

    print("The Odyssey IMAX 70mm seat checker", flush=True)
    print("Alert criteria:", flush=True)
    for theater in theaters:
        pair_row = minimum_pair_row(theater, args.pair_min_row)
        print(
            f"  {theater.name}: 2 together in physical row {pair_row}+.",
            flush=True,
        )
    print(
        "Ratings: 0-100 seat quality; center 60%, row placement 40%. "
        f"Off-hour windows apply -{OFF_HOUR_TIME_PENALTY} to the final score.",
        flush=True,
    )
    print(
        f"Minimum shown/alerted rating: {MINIMUM_SHOWN_RATING}/100.",
        flush=True,
    )
    print(
        "Colors: "
        + ", ".join(
            (
                f"NO DEAL below {MINIMUM_SHOWN_RATING} (hidden)",
                color_by_rating(
                    f"EXCELLENT {EXCELLENT_MIN_SCORE}-100",
                    EXCELLENT_MIN_SCORE,
                ),
                color_by_rating(
                    f"GOOD {GOOD_MIN_SCORE}-{EXCELLENT_MIN_SCORE - 1}",
                    GOOD_MIN_SCORE,
                ),
            )
        ),
        flush=True,
    )
    print(
        "Theaters: " + ", ".join(theater.name for theater in theaters),
        flush=True,
    )
    print(
        "Dates: " + ", ".join(target_date.isoformat() for target_date in dates),
        flush=True,
    )
    if args.target_time:
        target_description = ", ".join(
            target.strftime("%I:%M %p").lstrip("0")
            + (
                f" (off-hour, -{OFF_HOUR_TIME_PENALTY})"
                if target_has_time_penalty(target)
                else ""
            )
            for target in targets
        )
    else:
        target_description = (
            f"11:00 AM-12:00 PM (off-hour, -{OFF_HOUR_TIME_PENALTY}), "
            "3:00 PM, 7:00 PM, "
            f"11:00 PM-midnight (off-hour, -{OFF_HOUR_TIME_PENALTY})"
        )
    print("Target times: " + target_description, flush=True)
    print(
        "Browser: "
        f"{args.browser_channel} ({'visible' if args.headed else 'headless'}). "
        "No seats will be selected and checkout will not be opened.",
        flush=True,
    )

    async with async_playwright() as playwright:
        launch_options: dict[str, Any] = {"headless": not args.headed}
        if args.browser_channel != "chromium":
            launch_options["channel"] = args.browser_channel
        browser = await playwright.chromium.launch(**launch_options)
        context = await browser.new_context(
            viewport={"width": 1440, "height": 1000},
            locale="en-US",
        )
        page = await context.new_page()

        while True:
            dates = dates_not_in_past(requested_dates)
            if not dates:
                print(
                    "\nNo current or future dates remain; stopping.",
                    flush=True,
                )
                break
            print(
                "\nStarting scan at "
                + datetime.now().astimezone().strftime(
                    "%Y-%m-%d %I:%M:%S %p %Z"
                ),
                flush=True,
            )
            results, errors = await check_cycle(
                page=page,
                theaters=theaters,
                dates=dates,
                targets=targets,
                pair_min_row=args.pair_min_row,
                window_minutes=args.window_minutes,
                timeout_ms=timeout_ms,
                delay_seconds=args.delay_seconds,
            )
            current_keys: set[str] = set()
            for result in results:
                current_keys.update(availability_keys(result))
            new_keys = current_keys - previous_keys
            if new_keys:
                audible_alert()
                print(
                    terminal_color(
                        f"ALERT: {len(new_keys)} new qualifying "
                        "availability item(s).",
                        ANSI_BRIGHT_GREEN,
                    ),
                    flush=True,
                )
            previous_keys = current_keys
            print_cycle(results, errors)

            if not args.repeat_minutes:
                break
            next_check = datetime.now().astimezone() + timedelta(
                minutes=args.repeat_minutes
            )
            print(
                f"\nWaiting {args.repeat_minutes:g} minutes. Next scan at "
                f"{next_check:%Y-%m-%d %I:%M:%S %p %Z}. "
                "Press Ctrl+C to stop.",
                flush=True,
            )
            await asyncio.sleep(args.repeat_minutes * 60)

        await context.close()
        await browser.close()
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.self_test:
        run_self_test()
        return 0
    try:
        return asyncio.run(async_main(args))
    except KeyboardInterrupt:
        print("\nStopped.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
