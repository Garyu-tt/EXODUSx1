import unittest
from exodus.performance import compare, metrics


class PerformanceTests(unittest.TestCase):
    def test_constant(self):
        result = compare([20.0] * 100, [10.0] * 100)
        self.assertEqual(result['baseline']['mean_fps'], 50)
        self.assertEqual(result['delta_percent']['mean_fps'], 100)
        self.assertEqual(result['delta_percent']['p99_frame_ms'], -50)

    def test_tail(self):
        result = metrics([10.0] * 99 + [100.0])
        self.assertEqual(result['one_percent_low_fps'], 10)
        self.assertEqual(result['p99_frame_ms'], 10)

    def test_invalid(self):
        for invalid in (0, -1, float('nan'), float('inf'), 60001):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                metrics([10.0] * 99 + [invalid])
        with self.assertRaises(ValueError):
            metrics([10.0] * 99)

    def test_equal(self):
        self.assertTrue(all(x == 0 for x in compare([16.0] * 100, [16.0] * 100)['delta_percent'].values()))


if __name__ == '__main__':
    unittest.main()
