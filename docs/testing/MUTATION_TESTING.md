# Mutation testing report

## Command and method

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\mutation-test.ps1 -BuildDir build-audit-debug
```

The runner applies each exact-text mutation separately, builds its target,
runs its focused CTest or pytest selection and restores the original source
bytes in a `finally` block. Line endings are normalized only while matching
anchors. The original file bytes are restored after each attempt.

## Measured result

| Result | Count |
|---|---:|
| Generated | 17 |
| Killed | 17 |
| Survived | 0 |
| Excluded | 0 |
| Mutation score | 100% |

All 17 mutations were killed. The set covers speed and teleport boundaries,
aim angular-rate and grace rules, sequence ordering, token-bucket boundaries,
HTTPS client-certificate authorization and benchmark evidence validation.
No equivalent or excluded mutant remains.

The detailed machine-readable output is `artifacts/mutation-report.json` in
the ignored `artifacts/` directory. The runner exits nonzero if any mutant
survives or is excluded.
