# F-NOVELTY Ablation: Novelty Reward On vs Off

## Experimental Setup
- **Population**: 24 agents, NEAT brains, coevolution mode
- **Generations**: 200, Episode ticks: 200
- **Seeds**: 3 per mode (42, 43, 44)
- **Key difference**: `novelty_enabled = true` (weight=5.0) vs `novelty_enabled = false`

## Results

| Metric | Novelty ON | Novelty OFF |
|--------|-----------|-------------|
| Final avg best_fitness | 1456.6 | 79.3 |
| Peak avg best_fitness | 4998.6 | 475.9 |
| Staircase patterns detected | 0 | 0 |

## Fitness Curve (sampled every 25 gen)

| Gen | Novelty ON | Novelty OFF |
|-----|-----------|-------------|
| 0 | 1994.6 | 288.8 |
| 25 | 1808.3 | 217.3 |
| 50 | 2539.2 | 220.4 |
| 75 | 1598.6 | 168.3 |
| 100 | 1620.9 | 133.5 |
| 125 | 2512.7 | 133.8 |
| 150 | 1236.9 | 167.9 |
| 175 | 1522.3 | 191.5 |
| 199 | 1456.6 | 79.3 |

## Analysis

Both modes show similar staircase counts (0 vs 0).

Novelty ON achieves higher final fitness (1456.6 vs 79.3).

## Conclusion
Novelty reward shows measurable benefit, supporting F-NOVELTY's role in maintaining exploration pressure.
