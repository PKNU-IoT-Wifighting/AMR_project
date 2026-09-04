import sqlite3
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

from navigation_history import NavigationHistoryRepository


class NavigationHistoryRepositoryTest(unittest.TestCase):
    def test_start_and_finish_navigation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database_path = Path(directory) / "nested" / "navigation.db"
            repository = NavigationHistoryRepository(database_path)

            history_id = repository.start("restroom", "화장실 앞")

            with sqlite3.connect(database_path) as connection:
                started = connection.execute(
                    "SELECT destination_id, destination_name, started_at, ended_at "
                    "FROM navigation_history WHERE id = ?",
                    (history_id,),
                ).fetchone()
            self.assertEqual(started[0:2], ("restroom", "화장실 앞"))
            self.assertIsNotNone(started[2])
            self.assertEqual(datetime.fromisoformat(started[2]).utcoffset().total_seconds(), 9 * 3600)
            self.assertIsNone(started[3])

            repository.finish(history_id, "arrived")

            with sqlite3.connect(database_path) as connection:
                ended_at, outcome = connection.execute(
                    "SELECT ended_at, outcome FROM navigation_history WHERE id = ?",
                    (history_id,),
                ).fetchone()
            self.assertIsNotNone(ended_at)
            self.assertEqual(datetime.fromisoformat(ended_at).utcoffset().total_seconds(), 9 * 3600)
            self.assertEqual(outcome, "arrived")

    def test_existing_utc_timestamps_are_migrated_to_korean_time(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database_path = Path(directory) / "navigation.db"
            NavigationHistoryRepository(database_path)
            with sqlite3.connect(database_path) as connection:
                connection.execute(
                    """
                    INSERT INTO navigation_history (
                        destination_id, destination_name, started_at, ended_at
                    ) VALUES (?, ?, ?, ?)
                    """,
                    (
                        "restroom",
                        "화장실 앞",
                        "2026-09-03T00:00:00.000+00:00",
                        "2026-09-03T00:10:00.000+00:00",
                    ),
                )

            NavigationHistoryRepository(database_path)
            with sqlite3.connect(database_path) as connection:
                started_at, ended_at = connection.execute(
                    "SELECT started_at, ended_at FROM navigation_history"
                ).fetchone()

            self.assertEqual(started_at, "2026-09-03T09:00:00.000+09:00")
            self.assertEqual(ended_at, "2026-09-03T09:10:00.000+09:00")

    def test_list_recent_returns_newest_first(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repository = NavigationHistoryRepository(Path(directory) / "navigation.db")
            first_id = repository.start("restroom", "화장실 앞")
            repository.finish(first_id, "canceled")
            second_id = repository.start("room_301", "강의실 301호")

            records = repository.list_recent()

            self.assertEqual([record["id"] for record in records], [second_id, first_id])
            self.assertEqual(records[0]["destination_name"], "강의실 301호")
            self.assertIsNone(records[0]["ended_at"])
            self.assertEqual(records[1]["outcome"], "canceled")

    def test_existing_database_gets_outcome_column(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database_path = Path(directory) / "navigation.db"
            with sqlite3.connect(database_path) as connection:
                connection.execute(
                    """
                    CREATE TABLE navigation_history (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        destination_id TEXT NOT NULL,
                        destination_name TEXT NOT NULL,
                        started_at TEXT NOT NULL,
                        ended_at TEXT
                    )
                    """
                )

            repository = NavigationHistoryRepository(database_path)
            history_id = repository.start("elevator", "엘리베이터 앞")
            repository.finish(history_id, "arrived")

            self.assertEqual(repository.list_recent()[0]["outcome"], "arrived")


if __name__ == "__main__":
    unittest.main()
