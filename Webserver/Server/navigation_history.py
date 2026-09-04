"""SQLite storage for completed and in-progress navigation sessions."""

from __future__ import annotations

import sqlite3
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo


KOREA_TIMEZONE = ZoneInfo("Asia/Seoul")


def korea_now_iso() -> str:
    """Return the current Korean time as a timezone-aware ISO timestamp."""
    return datetime.now(KOREA_TIMEZONE).isoformat(timespec="milliseconds")


def _as_korea_time(value: str | None) -> str | None:
    if value is None:
        return None
    timestamp = datetime.fromisoformat(value)
    if timestamp.tzinfo is None:
        return value
    return timestamp.astimezone(KOREA_TIMEZONE).isoformat(timespec="milliseconds")


class NavigationHistoryRepository:
    """Persist navigation lifecycle events using short-lived connections."""

    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path
        self.database_path.parent.mkdir(parents=True, exist_ok=True)
        self._initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.database_path, timeout=5.0)
        connection.execute("PRAGMA busy_timeout = 5000")
        return connection

    def _initialize(self) -> None:
        with self._connect() as connection:
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS navigation_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    destination_id TEXT NOT NULL,
                    destination_name TEXT NOT NULL,
                    started_at TEXT NOT NULL,
                    ended_at TEXT,
                    outcome TEXT
                )
                """
            )
            columns = {
                row[1]
                for row in connection.execute(
                    "PRAGMA table_info(navigation_history)"
                ).fetchall()
            }
            if "outcome" not in columns:
                connection.execute(
                    "ALTER TABLE navigation_history ADD COLUMN outcome TEXT"
                )
            connection.execute(
                """
                CREATE INDEX IF NOT EXISTS idx_navigation_history_started_at
                ON navigation_history(started_at)
                """
            )
            # Older versions stored timezone-aware UTC values. Convert those
            # existing rows to the equivalent Asia/Seoul representation.
            rows = connection.execute(
                """
                SELECT id, started_at, ended_at
                FROM navigation_history
                WHERE started_at LIKE '%+00:00'
                   OR ended_at LIKE '%+00:00'
                """
            ).fetchall()
            for history_id, started_at, ended_at in rows:
                connection.execute(
                    """
                    UPDATE navigation_history
                    SET started_at = ?, ended_at = ?
                    WHERE id = ?
                    """,
                    (
                        _as_korea_time(started_at),
                        _as_korea_time(ended_at),
                        history_id,
                    ),
                )

    def start(self, destination_id: str, destination_name: str) -> int:
        with self._connect() as connection:
            cursor = connection.execute(
                """
                INSERT INTO navigation_history (
                    destination_id, destination_name, started_at
                ) VALUES (?, ?, ?)
                """,
                (destination_id, destination_name, korea_now_iso()),
            )
            if cursor.lastrowid is None:
                raise RuntimeError("안내 이력 ID를 생성하지 못했습니다.")
            return cursor.lastrowid

    def finish(self, history_id: int, outcome: str) -> None:
        with self._connect() as connection:
            connection.execute(
                """
                UPDATE navigation_history
                SET ended_at = ?, outcome = ?
                WHERE id = ? AND ended_at IS NULL
                """,
                (korea_now_iso(), outcome, history_id),
            )

    def list_recent(self, limit: int = 200) -> list[dict[str, object]]:
        """Return the newest navigation sessions first."""
        safe_limit = max(1, min(limit, 500))
        with self._connect() as connection:
            connection.row_factory = sqlite3.Row
            rows = connection.execute(
                """
                SELECT id, destination_id, destination_name, started_at, ended_at,
                       outcome
                FROM navigation_history
                ORDER BY id DESC
                LIMIT ?
                """,
                (safe_limit,),
            ).fetchall()
        return [dict(row) for row in rows]
