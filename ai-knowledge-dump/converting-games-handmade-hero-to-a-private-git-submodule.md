# Git Submodules Guide

A complete reference for managing submodules in `de100-game-engine`.

## Table of Contents

1. [Daily Workflows](#1-daily-workflows)
2. [Adding a New Submodule](#2-adding-a-new-submodule)
3. [Removing a Submodule](#3-removing-a-submodule)
4. [Moving, Renaming, and Reconfiguring Submodules](#4-moving-renaming-and-reconfiguring-submodules)
5. [Multi-Machine Workflows](#5-multi-machine-workflows)
6. [Detaching a Submodule](#6-detaching-a-submodule-make-it-standalone)
7. [Mixed Public/Private Submodules](#7-mixed-publicprivate-submodules)
8. [Collaboration Scenarios](#8-collaboration-scenarios)
9. [README Documentation Templates](#9-readme-documentation-templates)
10. [Quick Reference](#10-quick-reference)
11. [Advanced Git Operations](#11-advanced-git-operations)
12. [Troubleshooting](#12-troubleshooting)

---

## Current Structure

| Repository                        | Visibility | Path                        | Description                  |
| --------------------------------- | ---------- | --------------------------- | ---------------------------- |
| `DreamEcho100/de100-game-engine`  | PUBLIC     | `/`                         | Engine core                  |
| `DreamEcho100/handmade-hero-game` | PRIVATE    | `games/handmade-hero-game/` | Handmade Hero implementation |

---

## 1. Daily Workflows

### 1.0 Understanding "Detached HEAD" in Submodules

When you clone or run `git submodule update`, submodules are in **detached HEAD** state:

```
❯ cd games/handmade-hero-game
❯ git status
HEAD detached at ee57439    ← This is NORMAL!
```

**Why?** The parent repo records a specific commit hash, not a branch. Git checks out that exact commit.

**When is this OK?**

- ✅ Reading/browsing code
- ✅ Building
- ✅ Testing
- ❌ Making changes (commits would be "orphaned")

**Before making changes, attach to a branch:**

```bash
cd games/handmade-hero-game
git checkout main  # Now you can commit normally
```

---

### 1.1 Working on Main Repo ONLY (Engine)

You're editing engine code, not touching games:

```bash
# Edit engine files
vim engine/game/base.h

# Commit and push (submodules unchanged)
git add engine/
git commit -m "feat: add new engine feature"
git push
```

**Submodules are unaffected** - they stay at their recorded commits.

---

### 1.2 Working on Submodule ONLY (Game)

You're editing game code, engine stays the same:

```bash
# 1. Enter submodule and checkout a branch (exit detached HEAD!)
cd games/handmade-hero-game
git checkout main  # ← IMPORTANT: attach to branch first!

# 2. Make changes
vim src/main.c

# 3. Commit and push INSIDE the submodule
git add . && git commit -m "feat: game changes"
git push origin main

# 4. Go back to parent and update its reference
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update handmade-hero-game"
git push
```

**Why step 4?** The parent repo stores which commit each submodule points to. After pushing submodule changes, you need to update this pointer.

---

### 1.3 Working on BOTH (Engine + Game) Simultaneously

You're making coordinated changes across engine and game:

```bash
# 1. Make engine changes
vim engine/game/base.h

# 2. Make game changes (checkout branch first!)
cd games/handmade-hero-game
git checkout main
vim src/main.c

# 3. Commit and push the SUBMODULE first
git add . && git commit -m "feat: use new engine API"
git push origin main

# 4. Go back to parent
cd ../..

# 5. Commit BOTH the engine changes AND the submodule pointer
git add engine/ games/handmade-hero-game
git commit -m "feat: add new engine feature + update game to use it"
git push
```

**Order matters:** Push submodule BEFORE committing parent, so the commit hash exists on remote.

---

### 1.4 Working on Multiple Submodules

If you have several games:

```bash
# Edit game A
cd games/game-a
git checkout main
vim src/main.c
git add . && git commit -m "feat: game-a changes" && git push
cd ../..

# Edit game B
cd games/game-b
git checkout main
vim src/main.c
git add . && git commit -m "feat: game-b changes" && git push
cd ../..

# Update parent to point to both new commits
git add games/game-a games/game-b
git commit -m "chore: update game-a and game-b"
git push
```

---

### 1.5 Pulling Everything (Main + All Submodules)

```bash
git pull
git submodule update --recursive
```

**Note:** After `git submodule update`, submodules are back in detached HEAD. If you were working on a submodule, you'll need to `git checkout main` again.

---

### 1.6 Pulling a Specific Submodule to Latest

```bash
cd games/handmade-hero-game
git checkout main  # Make sure you're on a branch
git pull origin main
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update handmade-hero-game to latest"
git push
```

---

### 1.7 Cloning on a Fresh Machine

```bash
# Full clone with all submodules
git clone --recursive git@github.com:DreamEcho100/de100-game-engine.git

# Or init after clone
git clone git@github.com:DreamEcho100/de100-game-engine.git
cd de100-game-engine
git submodule update --init --recursive

# ⚠️ Submodules are now in detached HEAD - checkout branches before working:
cd games/handmade-hero-game
git checkout main
```

---

## 2. Adding a New Submodule

### 2.1 From Existing Directory (Convert to Submodule)

```bash
# 1. Copy directory out
cp -r games/my-game /tmp/my-game
cd /tmp/my-game

# 2. Create repo (choose visibility)
git init && git add . && git commit -m "Initial commit"
gh repo create DreamEcho100/my-game --private --source=. --push
# OR: --public for public repos

# 3. Back in main repo - remove original directory
cd /path/to/de100-game-engine
git rm -r games/my-game
git commit -m "chore: prepare my-game for submodule conversion"

# 4. Add as submodule - ALWAYS SPECIFY THE PATH!
#    ⚠️ If you omit the path, it creates at repo root with repo name
git submodule add git@github.com:DreamEcho100/my-game.git games/my-game
#                 └── URL ──────────────────────────────────┘ └── PATH ┘

# 5. Verify it worked
git submodule status
# Should show: -<commit_hash> games/my-game

# 6. Test the build
./games/my-game/build-dev.sh  # Or whatever your build command is

# 7. Commit and push
git commit -m "chore: add my-game as submodule"
git push
```

### 2.2 Add Existing Remote Repo as Submodule

```bash
# ALWAYS specify the path to avoid surprises
git submodule add git@github.com:SomeUser/some-repo.git path/to/submodule
git commit -m "chore: add some-repo submodule"
git push
```

### 2.3 When Repo Name ≠ Directory Name

If your repo is `my-game-private` but you want directory `games/my-game`:

```bash
# The path argument controls the local directory name
git submodule add git@github.com:DreamEcho100/my-game-private.git games/my-game
#                                              └── repo name ──┘       └ dir ┘

# .gitmodules will show:
# [submodule "my-game"]           <-- uses last path component as name
#     path = games/my-game        <-- your chosen path
#     url = .../my-game-private   <-- actual repo
```

### 2.4 ⚠️ Common Mistake: Submodule Added to Wrong Location

**Problem:** You ran `git submodule add <url>` without the path, and now you have:

```
de100-game-engine/
├── handmade-hero-game/    ← Wrong! Should be in games/
├── games/
│   └── ...
```

**Solution:** Use `git mv` to move it:

```bash
# Move the submodule to correct location
git mv handmade-hero-game games/handmade-hero-game

# Verify
git submodule status
cat .gitmodules  # Should show updated path

# Commit the move
git commit -m "chore: move submodule to correct location"
git push
```

### 2.5 Configure `.gitmodules`

The file is auto-generated but you can edit it:

```ini
[submodule "my-game"]
    path = games/my-game
    url = git@github.com:DreamEcho100/my-game.git
    branch = main  # Optional: track specific branch
```

---

## 3. Removing a Submodule

```bash
# 1. Deinit (removes from .git/config)
git submodule deinit -f games/my-game

# 2. Remove from index and working tree
git rm -f games/my-game

# 3. Remove cached module data
rm -rf .git/modules/games/my-game

# 4. Commit
git commit -m "chore: remove my-game submodule"
git push
```

**Note:** The remote repo still exists - this just unlinks it.

---

## 4. Moving, Renaming, and Reconfiguring Submodules

### 4.1 Move a Submodule to Different Location

```bash
# Simple move - git handles .gitmodules and .git/config automatically
git mv games/old-location games/new-location
git commit -m "chore: move submodule to new location"

# Verify
git submodule status
```

### 4.2 Rename a Submodule (Change the Internal Name)

The submodule "name" is the key in `.gitmodules` (e.g., `[submodule "my-game"]`).
To rename it, you need to remove and re-add:

```bash
# 1. Note current URL and commit
git config --file .gitmodules submodule.old-name.url
cd games/my-game && git rev-parse HEAD && cd ../..

# 2. Remove (but keep files)
git submodule deinit -f games/my-game
git rm -f games/my-game
rm -rf .git/modules/games/my-game

# 3. Re-add with new name (name comes from path)
git submodule add git@github.com:DreamEcho100/my-game.git games/new-name
git commit -m "chore: rename submodule from old-name to new-name"
```

### 4.3 Change Submodule Remote URL

If the remote repo moved (e.g., username change, repo rename):

```bash
# Method 1: Edit .gitmodules directly
vim .gitmodules
# Change: url = git@github.com:OLD/repo.git
# To:     url = git@github.com:NEW/repo.git

# Then sync the change
git submodule sync
git submodule update --init --recursive

# Commit
git add .gitmodules
git commit -m "chore: update submodule URL"
```

```bash
# Method 2: Use git config
git config --file .gitmodules submodule.my-game.url git@github.com:NEW/repo.git
git submodule sync
git add .gitmodules
git commit -m "chore: update submodule URL"
```

### 4.4 Switch Submodule Branch

```bash
# Set tracked branch in .gitmodules
git config --file .gitmodules submodule.my-game.branch develop

# Update to that branch
git submodule update --remote games/my-game

git add .gitmodules games/my-game
git commit -m "chore: switch submodule to develop branch"
```

---

## 5. Multi-Machine Workflows

### 5.1 Keep Machines in Sync

Both machines should track the same submodule commits:

```bash
# Machine A: After making submodule changes
cd games/handmade-hero-game
git add . && git commit -m "feat: changes" && git push
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule" && git push

# Machine B: Pull everything
git pull
git submodule update --recursive
```

### 5.2 Work on Different Submodule Versions (Intentional Divergence)

If you want Machine A and B to have different submodule states:

```bash
# Machine A: Use commit abc123
cd games/handmade-hero-game
git checkout abc123

# Machine B: Use commit def456
cd games/handmade-hero-game
git checkout def456

# DON'T commit the submodule pointer change to main repo
# Or use different branches in main repo
```

### 5.3 Avoid Conflicts

**Golden rule:** Don't commit submodule pointer changes from both machines simultaneously.

```bash
# Before working on submodule, always:
git pull
git submodule update --recursive

# If conflict happens in submodule pointer:
git checkout --theirs games/handmade-hero-game  # Accept remote
# OR
git checkout --ours games/handmade-hero-game    # Keep local
git add games/handmade-hero-game
git commit
```

### 5.4 Working Offline / Without Access to Private Submodule

```bash
# Clone without private submodules (public engine only)
git clone https://github.com/DreamEcho100/de100-game-engine.git
# Submodule init will fail for private repos - that's fine

# Later, when you have access:
git submodule update --init games/handmade-hero-game
```

---

## 6. Detaching a Submodule (Make It Standalone)

To convert a submodule back to a regular directory (keeping the repo intact):

```bash
# 1. Note the current submodule remote URL
git config --file .gitmodules submodule.handmade-hero-game.url
# Output: git@github.com:DreamEcho100/handmade-hero-game.git

# 2. Deinit and remove submodule tracking
git submodule deinit -f games/handmade-hero-game
git rm -f games/handmade-hero-game
rm -rf .git/modules/games/handmade-hero-game

# 3. Clone as regular directory (now part of main repo)
git clone git@github.com:DreamEcho100/handmade-hero-game.git games/handmade-hero-game
rm -rf games/handmade-hero-game/.git  # Remove nested git

# 4. Add as regular files
git add games/handmade-hero-game
git commit -m "chore: convert handmade-hero-game from submodule to directory"
```

**The remote repo `DreamEcho100/handmade-hero-game` still exists independently.**

---

## 7. Mixed Public/Private Submodules

### 7.1 Access Matrix

| Submodule Type | Clone with HTTPS | Clone with SSH | Requires Auth |
| -------------- | ---------------- | -------------- | ------------- |
| Public         | ✅ Anyone        | ✅ Anyone      | No            |
| Private        | ❌ Fails         | ✅ With access | Yes (SSH key) |

### 7.2 Graceful Handling for Contributors Without Access

In build scripts:

```bash
check_submodule() {
    local path="$1"
    if [[ ! -d "$path/.git" && ! -f "$path/.git" ]]; then
        echo "⚠️  Submodule '$path' not available (private or not initialized)"
        echo "   Run: git submodule update --init $path"
        echo "   (Requires repository access)"
        return 1
    fi
    return 0
}

# Usage
if check_submodule "games/handmade-hero-game"; then
    ./games/handmade-hero-game/build-dev.sh
else
    echo "Skipping private game build"
fi
```

### 7.3 `.gitmodules` with Mixed Visibility

```ini
[submodule "handmade-hero-game"]
    path = games/handmade-hero-game
    url = git@github.com:DreamEcho100/handmade-hero-game.git
    # Private - requires SSH access

[submodule "example-game"]
    path = games/example-game
    url = https://github.com/DreamEcho100/example-game.git
    # Public - anyone can clone
```

---

## 8. Collaboration Scenarios

### 8.1 Solo Developer

- Push submodule first, then main repo
- Use `git submodule update --remote` to pull latest

### 8.2 Team / Open Source

**Submodule PR workflow:**

```bash
# Contributor forks submodule repo
# Makes changes, opens PR to submodule
# After merge:

# Maintainer updates main repo
cd games/handmade-hero-game
git pull origin main
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update handmade-hero-game (PR #123)"
git push
```

### 8.3 Public Engine + Private Games (Your Setup)

- Engine: Accept PRs, anyone can contribute
- Games: Only collaborators with explicit access
- Document in README which submodules are private

### 8.4 Corporate / Closed Source

- All submodules private
- Use deploy keys or GitHub App for CI
- Document access requirements in CONTRIBUTING.md

---

## 9. README Documentation Templates

### 9.1 Main Repo README Section

Add to `de100-game-engine/README.md`:

```markdown
## 🎮 Games

Games are stored as git submodules. Some are private.

| Game                       | Status     | Description                                     |
| -------------------------- | ---------- | ----------------------------------------------- |
| `games/handmade-hero-game` | 🔒 Private | Handmade Hero following Casey Muratori's series |
| `games/example-game`       | 🌍 Public  | Demo game for learning                          |

### Cloning

\`\`\`bash

# With all submodules (need access to private ones)

git clone --recursive git@github.com:DreamEcho100/de100-game-engine.git

# Engine only (works for everyone)

git clone https://github.com/DreamEcho100/de100-game-engine.git
\`\`\`

### No Private Access?

You can still:

- Use and modify the engine
- Create your own game in `games/your-game/`
- Contribute to public submodules
```

### 9.2 Submodule Repo README

Add to `handmade-hero-game/README.md`:

```markdown
# Handmade Hero Game

🔒 **Private submodule** of [de100-game-engine](https://github.com/DreamEcho100/de100-game-engine)

## Setup

This repo is meant to be used as a submodule:

\`\`\`bash
cd /path/to/de100-game-engine
git submodule update --init games/handmade-hero-game
\`\`\`

## Building

\`\`\`bash
cd games/handmade-hero-game
./build-dev.sh
\`\`\`

## Structure

- `src/` - Game source code
- `build-dev.sh` - Build script (uses parent engine)
```

---

## 10. Quick Reference

### Commands Cheatsheet

| Task                              | Command                                                  |
| --------------------------------- | -------------------------------------------------------- |
| Clone with submodules             | `git clone --recursive <url>`                            |
| Init submodules after clone       | `git submodule update --init --recursive`                |
| Pull everything                   | `git pull && git submodule update --recursive`           |
| Update submodule to latest remote | `cd submodule && git pull && cd .. && git add submodule` |
| Add new submodule                 | `git submodule add <url> <path>`                         |
| Remove submodule                  | `git submodule deinit -f <path> && git rm -f <path>`     |
| Check submodule status            | `git submodule status`                                   |
| See submodule changes             | `git diff --submodule`                                   |

### Useful Aliases (`~/.gitconfig`)

```ini
[alias]
    # Pull + update submodules
    pull-all = !git pull && git submodule update --recursive

    # Update all submodules to latest remote
    sub-latest = submodule update --remote --recursive

    # Clone with submodules
    clone-full = clone --recursive

    # Status including submodules
    st = !git status && git submodule status
```

---

## 11. Advanced Git Operations

How standard git commands behave differently in main repo vs submodules.

### 11.1 Branching

#### Main Repo Branching

```bash
# Create and switch to feature branch
git checkout -b feature/new-engine-api

# Work on engine code
vim engine/game/base.h
git add . && git commit -m "feat: new API"

# Push feature branch
git push -u origin feature/new-engine-api

# Later: merge to main
git checkout main
git merge feature/new-engine-api
git push
```

**Submodules stay at their recorded commits** when you switch main repo branches.

#### Submodule Branching

```bash
# Enter submodule
cd games/handmade-hero-game

# Create feature branch
git checkout -b feature/new-gameplay

# Work and commit inside submodule
vim src/main.c
git add . && git commit -m "feat: new gameplay"
git push -u origin feature/new-gameplay

# Back to parent - record this commit
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule to feature branch"
git push
```

#### Working on Feature Branches in BOTH

```bash
# 1. Create feature branch in main repo
git checkout -b feature/coordinated-change

# 2. Create matching branch in submodule
cd games/handmade-hero-game
git checkout -b feature/coordinated-change
cd ../..

# 3. Work on both, commit separately
# ... make changes ...
cd games/handmade-hero-game
git add . && git commit -m "feat: game part" && git push -u origin feature/coordinated-change
cd ../..
git add . && git commit -m "feat: engine + game updates"
git push -u origin feature/coordinated-change

# 4. Merge both when ready
# First merge submodule
cd games/handmade-hero-game
git checkout main && git merge feature/coordinated-change && git push
cd ../..
# Then merge main repo
git checkout main && git merge feature/coordinated-change && git push
```

---

### 11.2 Rebase

#### Main Repo Rebase

```bash
# Rebase feature branch onto updated main
git checkout feature/my-feature
git fetch origin
git rebase origin/main

# Resolve conflicts if any, then:
git push --force-with-lease origin feature/my-feature
```

**⚠️ Warning:** If the rebase includes commits that changed submodule pointers, you may need to update submodules after:

```bash
git submodule update --recursive
```

#### Submodule Rebase

```bash
cd games/handmade-hero-game

# Rebase your feature branch onto updated main
git fetch origin
git rebase origin/main

# Force push after rebase
git push --force-with-lease origin feature/my-feature

# Back to parent - update pointer
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule after rebase"
git push
```

#### Interactive Rebase (Squash, Edit, Reorder)

```bash
# Main repo: last 3 commits
git rebase -i HEAD~3

# Submodule: same process
cd games/handmade-hero-game
git rebase -i HEAD~3
# After rebase, force push
git push --force-with-lease
# Update parent pointer
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule (rebased)"
```

---

### 11.3 Reset

#### Understanding Reset Modes

| Mode      | Moves HEAD | Changes Index | Changes Working Dir | Use Case                             |
| --------- | ---------- | ------------- | ------------------- | ------------------------------------ |
| `--soft`  | ✅         | ❌            | ❌                  | Undo commit, keep changes staged     |
| `--mixed` | ✅         | ✅            | ❌                  | Undo commit, keep changes unstaged   |
| `--hard`  | ✅         | ✅            | ✅                  | Completely undo, discard all changes |

#### Main Repo Reset

```bash
# Soft reset - undo last commit, keep changes staged
git reset --soft HEAD~1
# Now you can amend or re-commit differently

# Mixed reset (default) - undo commit, unstage changes
git reset HEAD~1
# Changes are in working directory, not staged

# Hard reset - completely undo last commit and discard changes
git reset --hard HEAD~1
# ⚠️ DANGER: Changes are GONE

# Reset to specific commit
git reset --hard abc1234

# Reset to match remote exactly
git fetch origin
git reset --hard origin/main
```

**⚠️ Submodule behavior during main repo reset:**

- Reset only changes the _pointer_ (which commit the submodule should be at)
- Submodule working directory is NOT automatically updated
- Run `git submodule update --recursive` after reset

```bash
git reset --hard HEAD~1
git submodule update --recursive  # ← Important!
```

#### Submodule Reset

```bash
cd games/handmade-hero-game

# Soft reset
git reset --soft HEAD~1

# Hard reset to discard all local changes
git reset --hard HEAD

# Reset to match remote
git fetch origin
git reset --hard origin/main

cd ../..
# If you want parent to point to this commit:
git add games/handmade-hero-game
git commit -m "chore: reset submodule to origin/main"
```

#### Reset Submodule to Parent's Expected Commit

If submodule is in a weird state and you want to reset it to what the parent expects:

```bash
# This resets submodule to the commit recorded in parent
git submodule update --force games/handmade-hero-game

# Or for all submodules:
git submodule update --force --recursive
```

---

### 11.4 Stash

#### Main Repo Stash

```bash
# Stash changes (engine files only, not submodule content)
git stash

# Do something else...

# Restore stashed changes
git stash pop

# Stash with message
git stash push -m "WIP: new feature"

# List stashes
git stash list

# Apply specific stash
git stash apply stash@{2}
```

**Note:** `git stash` in parent does NOT stash submodule changes.

#### Submodule Stash

```bash
cd games/handmade-hero-game

# Stash game changes
git stash push -m "WIP: gameplay tweak"

# Switch to different work...

# Come back and restore
git stash pop

cd ../..
```

#### Stash in Both

```bash
# Stash engine changes
git stash push -m "engine WIP"

# Stash game changes
cd games/handmade-hero-game
git stash push -m "game WIP"
cd ../..

# Later, restore both
cd games/handmade-hero-game
git stash pop
cd ../..
git stash pop
```

---

### 11.5 Cherry-Pick

#### Main Repo Cherry-Pick

```bash
# Apply a specific commit from another branch
git cherry-pick abc1234

# Cherry-pick without auto-commit
git cherry-pick -n abc1234
```

#### Submodule Cherry-Pick

```bash
cd games/handmade-hero-game

# Cherry-pick from submodule's other branch
git cherry-pick def5678
git push

cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule (cherry-picked commit)"
git push
```

---

### 11.6 Amend Last Commit

#### Main Repo Amend

```bash
# Fix last commit message
git commit --amend -m "better message"

# Add forgotten files to last commit
git add forgotten-file.c
git commit --amend --no-edit

# If already pushed, force push needed
git push --force-with-lease
```

#### Submodule Amend

```bash
cd games/handmade-hero-game
git commit --amend -m "fixed message"
git push --force-with-lease

# Parent now points to old (amended) commit hash!
# Update parent to point to new commit:
cd ../..
git add games/handmade-hero-game
git commit --amend --no-edit  # Or new commit
git push --force-with-lease
```

---

### 11.7 Log and History

#### Main Repo Log

```bash
# Standard log
git log --oneline -10

# Log with submodule changes shown
git log --oneline --submodule -10

# See what submodule commit changed
git log -p -- games/handmade-hero-game
```

#### Submodule Log

```bash
cd games/handmade-hero-game
git log --oneline -10
cd ../..
```

#### Log Across All Repos

```bash
# Show main repo log
echo "=== Main Repo ==="
git log --oneline -5

# Show submodule log
echo "=== Submodule ==="
cd games/handmade-hero-game && git log --oneline -5 && cd ../..
```

---

### 11.8 Tags

#### Main Repo Tags

```bash
# Create tag
git tag v1.0.0
git push origin v1.0.0

# Create annotated tag with message
git tag -a v1.0.0 -m "Release 1.0.0"
git push origin v1.0.0

# List tags
git tag -l
```

**Submodules:** Tags in parent repo capture the submodule commit pointer at that moment.

#### Submodule Tags

```bash
cd games/handmade-hero-game

# Tag the game version
git tag -a v1.0.0 -m "Game release 1.0.0"
git push origin v1.0.0

cd ../..
# Parent repo should also be tagged to mark this combination:
git tag -a v1.0.0 -m "Engine + Game release 1.0.0"
git push origin v1.0.0
```

#### Checkout a Tag

```bash
# Checkout main repo tag
git checkout v1.0.0

# Update submodules to what they were at that tag
git submodule update --recursive

# ⚠️ Both main repo and submodules are now in detached HEAD
```

---

### 11.9 Reverting Commits

#### Main Repo Revert

```bash
# Create a new commit that undoes a previous commit
git revert abc1234
git push
```

#### Submodule Revert

```bash
cd games/handmade-hero-game
git revert abc1234
git push

cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule (reverted commit)"
git push
```

---

### 11.10 Diff Operations

#### Main Repo Diff

```bash
# See uncommitted changes
git diff

# See staged changes
git diff --staged

# Diff with submodule summary
git diff --submodule
```

#### Submodule Diff

```bash
cd games/handmade-hero-game
git diff
cd ../..

# Or see all submodule diffs from parent:
git diff --submodule=diff
```

#### Diff Between Branches Including Submodules

```bash
# Shows which commits submodule moved between
git diff main..feature/branch --submodule
```

---

### 11.11 Summary Table

| Operation     | Main Repo                     | Submodule                               | Notes                                            |
| ------------- | ----------------------------- | --------------------------------------- | ------------------------------------------------ |
| Create branch | `git checkout -b branch`      | `cd sub && git checkout -b branch`      | Submodules track commits, not branches           |
| Rebase        | `git rebase origin/main`      | `cd sub && git rebase origin/main`      | Update parent pointer after submodule rebase     |
| Reset --soft  | `git reset --soft HEAD~1`     | `cd sub && git reset --soft HEAD~1`     | Parent pointer unchanged until you `git add`     |
| Reset --hard  | `git reset --hard HEAD~1`     | `cd sub && git reset --hard HEAD~1`     | Run `git submodule update` after main repo reset |
| Stash         | `git stash`                   | `cd sub && git stash`                   | Stashes are separate per repo                    |
| Cherry-pick   | `git cherry-pick abc`         | `cd sub && git cherry-pick abc`         | Update parent pointer after                      |
| Amend         | `git commit --amend`          | `cd sub && git commit --amend`          | Update parent pointer (commit hash changed!)     |
| Tag           | `git tag v1.0`                | `cd sub && git tag v1.0`                | Tag both for versioned releases                  |
| Force push    | `git push --force-with-lease` | `cd sub && git push --force-with-lease` | Always update parent after submodule force push  |

---

## 12. Troubleshooting

### "Submodule not initialized"

```bash
git submodule update --init --recursive
```

### "Permission denied" on private submodule

```bash
# Check SSH key is added
ssh -T git@github.com

# Verify you have access to the repo
gh repo view DreamEcho100/handmade-hero-game
```

### Submodule stuck on old commit

```bash
cd games/handmade-hero-game
git fetch origin
git checkout main
git pull origin main
cd ../..
git add games/handmade-hero-game
git commit -m "chore: update submodule to latest"
```

### Detached HEAD in submodule

**This is normal!** See [Section 1.0](#10-understanding-detached-head-in-submodules) for full explanation.

Quick fix before making changes:

```bash
cd games/handmade-hero-game
git checkout main  # Attach to branch, then you can commit
```

### Submodule added to wrong location

If you ran `git submodule add <url>` without specifying the path:

```bash
# Move it to correct location
git mv handmade-hero-game games/handmade-hero-game

# Verify .gitmodules was updated
cat .gitmodules

# Commit the fix
git commit -m "chore: move submodule to correct location"
```

### Submodule directory exists but is empty

```bash
# The submodule was registered but not cloned
git submodule update --init games/handmade-hero-game
```

### "fatal: not a git repository" inside submodule

The `.git` file (which points to parent's `.git/modules/`) is missing or corrupted:

```bash
# Re-init the submodule
git submodule deinit -f games/handmade-hero-game
git submodule update --init games/handmade-hero-game
```

### Changes in submodule not showing in `git status`

Submodule changes show as "modified content" or "new commits". To see details:

```bash
git diff --submodule
# Or:
git status --recurse-submodules
```

### "Please make sure you have the correct access rights"

```bash
# Verify SSH is working
ssh -T git@github.com

# Check the URL in .gitmodules uses SSH (not HTTPS for private repos)
cat .gitmodules | grep url

# If it's HTTPS, change to SSH:
git config --file .gitmodules submodule.handmade-hero-game.url git@github.com:DreamEcho100/handmade-hero-game.git
git submodule sync
```

### Submodule conflicts during merge/rebase

```bash
# Accept theirs (remote version)
git checkout --theirs games/handmade-hero-game
git add games/handmade-hero-game

# OR accept ours (local version)
git checkout --ours games/handmade-hero-game
git add games/handmade-hero-game

# Then continue merge/rebase
git merge --continue  # or git rebase --continue
```

---

## Appendix: Initial Setup (Historical Reference)

<details>
<summary>How handmade-hero-game was converted to a submodule</summary>

**What actually happened:**

```bash
# 1. Copied game directory to temp location
cp -r games/handmade-hero /tmp/handmade-hero-game
cd /tmp/handmade-hero-game

# 2. Created private repo with fresh history
git init && git add . && git commit -m "Initial commit"
gh repo create DreamEcho100/handmade-hero-game --private --source=. --push

# 3. Removed original directory from main repo
cd /path/to/de100-game-engine
git rm -r games/handmade-hero
git commit -m "chore: remove handmade-hero in preparation for submodule conversion"

# 4. Added submodule (initially to wrong location - see gotcha below!)
git submodule add git@github.com:DreamEcho100/handmade-hero-game.git
# ⚠️ This created 'handmade-hero-game/' at repo root (not in games/)

# 5. Moved to correct location
git mv handmade-hero-game games/handmade-hero-game

# 6. Committed and pushed
git commit -m "chore: add handmade-hero as private submodule"
git push
```

**Lesson learned:** Always specify the full path when adding a submodule:

```bash
# ❌ Wrong - creates at repo root with repo name
git submodule add git@github.com:User/repo.git

# ✅ Correct - creates at specified path
git submodule add git@github.com:User/repo.git path/to/submodule
```

</details>
