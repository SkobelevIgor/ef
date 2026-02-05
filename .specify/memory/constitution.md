<!--
Sync Impact Report
==================
Version change: (new) → 1.0.0
Modified principles: N/A (initial creation)
Added sections: Core Principles (3), Code Quality, Governance
Removed sections: Section 2 and Section 3 placeholders (replaced with Code Quality)
Templates requiring updates:
  - .specify/templates/plan-template.md: ✅ compatible (Constitution Check section exists)
  - .specify/templates/spec-template.md: ✅ compatible (no constitution-specific refs)
  - .specify/templates/tasks-template.md: ✅ compatible (no constitution-specific refs)
Follow-up TODOs: None
-->

# EF (Edit Files) Constitution

## Core Principles

### I. Terminal Native

All functionality MUST work within a standard terminal environment.
- The editor operates exclusively via terminal I/O using tcell
- No external GUI dependencies allowed
- MUST support standard terminal sizes and handle resize events
- All user interactions happen through keyboard input

### II. Vim-Style Modal Editing

The editor MUST follow Vim's modal editing paradigm.
- Normal, Insert, and Visual modes are the primary modes
- Mode transitions MUST be intuitive and consistent with Vim conventions
- Key bindings SHOULD align with Vim where practical
- Each mode has a distinct visual indicator (line number color)

### III. Data Integrity

User data MUST never be lost due to editor behavior.
- Auto-save MUST be enabled with reasonable delay (currently 200ms)
- Undo/redo history MUST be maintained during editing session
- File operations MUST handle errors gracefully without data corruption
- External file changes MUST be detected and handled appropriately

## Code Quality

- Code MUST compile without errors before committing
- Changes MUST be tested manually in a real terminal before completion
- Go idioms and standard library patterns SHOULD be preferred
- Dependencies MUST be minimal (currently only tcell)

## Governance

This constitution defines the non-negotiable principles for EF development:

1. All changes MUST comply with the three Core Principles
2. Amendments require explicit justification when principles conflict
3. Binary MUST be rebuilt after changes: `go build -o ef .`
4. Don't create a new branch for each new feature ( speckit.specify ). We have a gitflow where anything pushing to main without any PRs;

**Version**: 1.0.0 | **Ratified**: 2026-02-02 | **Last Amended**: 2026-02-02



