# FileOps & ProcOps Tools
## Directory Structure
- `bin/` - here are the executable binaries
- `src/` - here are the C source files
- `include/` - here are the C header files
- `data/` - here are persistent files
- `logs/` - here are execution logs
- `reports/` - here are the reports that are generated
- `tmp/` - here are temporary artifacts
- `tests/` - here are testing scenarios
- `doc/` - here is the documentation
- `tools/` - here are script tools

## How to use 
To execute the audit of T1, make sure you're in the root folder of this project and run:
```bash
./tools/t1_audit.sh
```
or
```bash
bash tools/t1_audit.sh
```
or
```bash
source . tools/t1_audit.sh
```

The generated reports will then be stored in `reports/`.
