# 🔊 Audio Learning Course: From Cargo-Culting to Competence

## Overview

This course transforms you from blindly copying ALSA code to truly understanding and owning your audio stack. Designed for full-stack web developers learning systems programming.

---

## Course Structure

### ✅ Unit 4: Error Handling & Debugging (5 lessons, ~5-7 hours)
Master ALSA error handling and debugging techniques:
- L4.1: ALSA Error Codes (60-90 min)
- L4.2: Day 9 vs Day 10 Strategy (90-120 min)
- L4.3: Initialization Failure Modes (60-90 min)
- L4.4: The Click Detector (45-60 min)
- L4.5: Debug Visualization (60-75 min)

**Competence Target:** Level 4 (Analysis - can debug failures)

---

### ✅ Unit 5: Advanced Audio Patterns (5 lessons, ~6-7.5 hours)
Implement advanced audio features from scratch:
- L5.1: Platform API Design (75-90 min)
- L5.2: Sample Buffer vs Secondary Buffer (60-75 min)
- L5.3: Sine Wave Math (75-90 min)
- L5.4: Panning & Volume (60-75 min)
- L5.5: Memory Management (75-90 min)

**Competence Target:** Level 5 (Synthesis - can implement from scratch)

---

### ✅ Unit 6: Porting & Alternative Backends (5 lessons, ~5.5-7 hours)
Port to PulseAudio, Web Audio, and make informed decisions:
- L6.1: Audio Backend Comparison Matrix (60-75 min)
- L6.2: PulseAudio Simple API Port (60-75 min)
- L6.3: Web Audio API Port (75-90 min)
- L6.4: Architecture Tradeoff Analysis (45-60 min)
- L6.5: Real-World Debugging War Story (60-75 min)

**Competence Target:** Level 6 (Evaluation - can critique and justify decisions)

---

## How to Use This Course

### Recommended Path

1. **Read PLAN.md** to understand the full vision
2. **Start with Unit 4, Lesson 1** (don't skip around)
3. **Do ALL exercises** in each lesson (hands-on is critical)
4. **Check your understanding** with the self-check quizzes
5. **Track progress** in UNITS-TRACKER.md
6. **Reflect** after each unit on what you learned

### Time Commitment

- **Total:** 17-20 hours over 2-3 weeks
- **Pace:** 2-3 hours/day recommended
- **Don't rush!** Understanding > speed

---

## Prerequisites

Before starting Units 4-6, you should have:
- ✅ Basic C programming knowledge
- ✅ Completed Units 1-3 (ALSA basics, ring buffers, game loop integration)
- ✅ Working ALSA audio code in your project
- ✅ Familiarity with your audio.c and backend.c files

---

## Learning Objectives (Overall)

By completing Units 4-6, you will be able to:

- ✅ **Debug** ALSA initialization failures independently
- ✅ **Diagnose** and fix audio clicks/pops
- ✅ **Understand** timing strategies (Day 9 vs Day 10)
- ✅ **Implement** audio synthesis (sine waves, panning, volume)
- ✅ **Design** platform-agnostic audio APIs
- ✅ **Port** to alternative backends (PulseAudio, Web Audio)
- ✅ **Analyze** tradeoffs between audio systems
- ✅ **Justify** architectural decisions to stakeholders
- ✅ **Explain** Casey's audio architecture to others

---

## File Structure

```
project/misc/audio/
├── README.md               ← You are here
├── PLAN.md                 ← Master plan (all 9 units)
├── COURSE-PROGRESS.md      ← Build status (for course creators)
├── COURSE-STATUS.md        ← Detailed progress tracking
├── UNITS-TRACKER.md        ← Your personal progress tracker
└── lessons/
    ├── unit-4/             ← Error Handling & Debugging
    ├── unit-5/             ← Advanced Audio Patterns
    └── unit-6/             ← Porting & Alternative Backends
```

---

## Lesson Format

Each lesson includes:

1. **Learning Objectives** (5-6 per lesson)
2. **Mystery Code Sections** (what you don't understand yet)
3. **Concept Explanations** (with diagrams and analogies)
4. **Code Examples** (from YOUR actual codebase)
5. **Hands-On Exercises** (modify code, observe results)
6. **Self-Check Quiz** (5 questions with answers)
7. **Key Takeaways** (summary tables)
8. **Connections** (to previous and next lessons)

---

## Tips for Success

### ✅ DO:
- **Type out code examples** (don't copy-paste)
- **Run every exercise** and observe the output
- **Debug deliberately** when things break (that's learning!)
- **Take notes** on "aha moments"
- **Review previous lessons** when confused
- **Ask questions** (on forums, Discord, etc.)

### ❌ DON'T:
- Skip exercises ("I'll come back later")
- Rush through quizzes
- Move on without understanding
- Compare your pace to others
- Give up when stuck (re-read the lesson)

---

## Support & Community

- **GitHub Issues:** Report typos, errors, or confusion
- **Discussions:** Share your progress and learnings
- **Casey's Handmade Hero:** Original source material

---

## Progress Tracking

Use `UNITS-TRACKER.md` to track:
- [ ] Lessons completed
- [ ] Exercises done
- [ ] Quiz scores
- [ ] Confidence level (1-10)
- [ ] Aha moments
- [ ] Time spent

---

## What's Next?

After completing Units 4-6, you can:

1. **Apply your knowledge** to your game project
2. **Continue to Unit 7:** Real-Time Systems (scheduling, priorities)
3. **Continue to Unit 8:** Performance Optimization (SIMD, lock-free)
4. **Continue to Unit 9:** Advanced Features (WAV loading, mixers)
5. **Teach someone else** (best way to solidify understanding!)

---

## Success Metrics

You'll know you've succeeded when:

- ✅ You can explain audio architecture without looking at notes
- ✅ Audio bugs don't intimidate you anymore
- ✅ You can choose backends based on requirements
- ✅ You understand WHY your code works, not just THAT it works
- ✅ You can port to a new backend in a day, not a week

---

## Final Words

**This course is hard.** You WILL struggle. That's the point!

Learning systems programming after web development requires:
- **Patience** (concepts are lower-level)
- **Persistence** (debugging is harder)
- **Practice** (doing > reading)

But when you finish, you'll **OWN this stack**.

No more cargo-culting. No more blind copying.

**You'll be a systems programmer.** 🚀

---

## Ready?

Start with: `lessons/unit-4/L4.1-alsa-error-codes.md`

**Let's do this!**

