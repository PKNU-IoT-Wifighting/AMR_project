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

            repository.finish(history_id)

            with sqlite3.connect(database_path) as connection:
                ended_at = connection.execute(
                    "SELECT ended_at FROM navigation_history WHERE id = ?",
                    (history_id,),
                ).fetchone()[0]
            self.assertIsNotNone(ended_at)
            self.assertEqual(datetime.fromisoformat(ended_at).utcoffset().total_seconds(), 9 * 3600)

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


if __name__ == "__main__":
    unittest.main()
