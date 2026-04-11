# Course Builder Improvements — LOATs Course Retrospective

## What worked well from course-builder.md

### 1. Three-phase workflow (Plan → Source → Lessons)
Building source files FIRST and testing them before writing any lessons was critical. The test harness caught several bugs in the intrusive list logic (circular sibling linking, free list reuse of `next_sibling`) that would have been invisible in a lessons-first approach. The source code is the ground truth; lessons describe what was already verified.

### 2. Dual-backend requirement
Forcing both X11 and Raylib backends to compile ensured the pool library had zero platform dependencies. `things.h`/`things.c` compiles with only `<stdbool.h>`, `<stdint.h>`, `<string.h>`, and `<assert.h>` — truly portable.

### 3. Standalone test harness before platform integration
The test harness (`test_harness.c`) was the primary development target for lessons 02-10. This made iteration fast (sub-second compile, terminal output) and kept the pool library completely decoupled from rendering/input.

## What was different about this course vs game courses

### 1. This is a LIBRARY course, not a game course
Game courses (Asteroids, Tetris, Snake) build toward a playable game. This course builds toward a reusable data structure. The game demo (lessons 11-12) is verification, not the goal. This required a different lesson structure: test harness output instead of visual milestones for 80% of the course.

### 2. Conceptual lessons exist (lesson 01)
Lesson 01 has no compilable code. It's pure ASCII diagrams and conceptual reasoning. The course-builder.md assumes every lesson produces compilable output — this deviation was necessary because the OOP problem discussion is foundational and needs space without the overhead of a build step.

### 3. No utils/ refactoring needed
Game courses typically start with inline drawing code and refactor to `utils/` in the final lesson. This course's game demo is minimal enough that inline `draw_rect` in `game.c` is sufficient. No refactoring lesson was needed.

### 4. No audio
The LOATs demo has no audio system. This is correct — audio would add complexity without teaching any pool concepts.

## What was missing from course-builder.md

### 1. No pattern for standalone library + test harness courses
The course-builder assumes a game course structure: window → drawing → input → game logic → polish. A library course needs: concept → implementation → test → iterate. The test harness pattern should be documented as a variant workflow.

### 2. No guidance for conceptual-only lessons
Lesson 01 has no code. The course-builder's requirement that "every lesson compilable" should have a documented exception for introductory/conceptual lessons that establish mental models.

### 3. No pattern for incremental struct evolution
The thing struct grows across lessons (16 → 20 → 28 → 32 → 48 bytes). The course-builder doesn't document how to handle structs that evolve across lessons where earlier lesson code has fewer fields than the final source. Recommendation: each lesson shows the struct AT THAT LESSON STAGE, and the final `things.h` in `course/src/` is the complete version.

## Concrete suggested improvements

### Before (current course-builder.md)
```
Every lesson must produce compilable output.
```

### After (suggested)
```
Every lesson must produce compilable output, EXCEPT:
- Lesson 01 (or equivalent introductory lesson) may be conceptual-only
  if it establishes mental models that are prerequisite for all subsequent
  code. Mark with: "> **Course Note:** This lesson is conceptual — no
  compilable code. We start coding in Lesson 02."
```

### Before
```
Phase 1 — Build the complete, fully working source code
```

### After
```
Phase 1 — Build the complete, fully working source code
  Variant: For LIBRARY courses (not game courses), the primary Phase 1
  artifact may be a standalone test harness (`test_harness.c`) that
  exercises the library API via terminal output. The game demo (if any)
  is a secondary integration target.
```

## Techniques from this course not in course-builder.md

| Technique | Where it appears | Should it be added? |
|---|---|---|
| Nil sentinel pattern | Lesson 06, things.h | Yes — useful for any entity pool course |
| Intrusive linked lists | Lesson 04-05, things.h | Yes — fundamental for game entity systems |
| Generational IDs | Lesson 09, things.h | Yes — standard for safe entity references |
| Back-of-napkin memory math | Lesson 02 | Yes — should be standard for any pool/array course |
| Test harness as primary build target | Lessons 02-10 | Yes — variant workflow for library courses |
| Instructor Voice Lock | Prompt-level | Yes — prevents tone drift in long courses |
| Engineering Principle Priority Order | Prompt-level | Yes — prevents compliance-over-insight failure mode |
