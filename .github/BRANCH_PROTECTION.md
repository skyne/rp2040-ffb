# Branch Protection Configuration

This document describes the required branch protection rules for the `main` branch.

## Required Status Checks

The following checks must pass before merging any pull request to `main`:

### Core Test Jobs (Required)

These jobs must complete successfully:

1. **Firmware Unit Tests (C++)** - `firmware-tests`
   - Compiles and runs firmware unit tests with Google Test
   - Generates coverage reports
   - Must pass with exit code 0

2. **Rust Backend Tests** - `rust-tests`
   - Runs Rust unit tests with cargo test
   - Generates coverage reports with cargo-llvm-cov
   - Must pass with exit code 0

3. **JavaScript Frontend Tests** - `javascript-tests`
   - Runs JavaScript unit tests with Vitest
   - Generates coverage reports
   - Must pass with exit code 0

4. **Test Summary** - `summary`
   - Aggregates results from all test jobs
   - Fails if any required test job failed
   - This is the primary status check gate

## GitHub Branch Protection Settings

To enable these checks as PR merge gates, configure the following in GitHub:

### Navigation
1. Go to: Repository Settings → Branches → Branch protection rules
2. Add rule for branch: `main`

### Required Settings

#### ✅ Require a pull request before merging
- [x] Require approvals: 1 (recommended)
- [x] Dismiss stale pull request approvals when new commits are pushed
- [x] Require approval of the most recent reviewable push

#### ✅ Require status checks to pass before merging
- [x] Require branches to be up to date before merging

**Required status checks:**
- `Test Summary` (from test.yml workflow)
- `firmware-tests` (from test.yml workflow)
- `rust-tests` (from test.yml workflow)
- `javascript-tests` (from test.yml workflow)

#### ✅ Additional Recommended Settings
- [x] Require conversation resolution before merging
- [x] Do not allow bypassing the above settings
- [x] Restrict who can push to matching branches (optional)

## Mutation Testing (Optional Gate)

The `mutation-testing` job runs on PRs but is **not required** for merge by default:
- It runs with `continue-on-error: true`
- Results are uploaded as artifacts for review
- Can be made required once mutation score thresholds are stable

To make mutation testing required:
1. Remove `continue-on-error: true` from mutation-testing steps
2. Add `mutation-testing` to the `needs` array in the `summary` job
3. Add `mutation-testing` to required status checks in GitHub

## Setting Up Branch Protection (Step-by-Step)

### Via GitHub UI

1. **Navigate to Settings**
   ```
   Repository → Settings → Branches
   ```

2. **Add Rule**
   - Click "Add branch protection rule"
   - Branch name pattern: `main`

3. **Enable Required Checks**
   - Check "Require status checks to pass before merging"
   - Check "Require branches to be up to date before merging"
   - Search and select these checks:
     - `Test Summary`
     - `firmware-tests`
     - `rust-tests`
     - `javascript-tests`

4. **Configure Pull Requests**
   - Check "Require a pull request before merging"
   - Set "Required number of approvals" to 1 or more
   - Check "Dismiss stale pull request approvals when new commits are pushed"

5. **Save Changes**
   - Click "Create" or "Save changes"

### Via GitHub CLI

```bash
# Enable branch protection with required checks
gh api repos/:owner/:repo/branches/main/protection \
  --method PUT \
  --field required_status_checks[strict]=true \
  --field required_status_checks[contexts][]=Test Summary \
  --field required_status_checks[contexts][]=firmware-tests \
  --field required_status_checks[contexts][]=rust-tests \
  --field required_status_checks[contexts][]=javascript-tests \
  --field required_pull_request_reviews[required_approving_review_count]=1 \
  --field required_pull_request_reviews[dismiss_stale_reviews]=true \
  --field enforce_admins=true \
  --field required_conversation_resolution=true
```

### Via Terraform (Infrastructure as Code)

```hcl
resource "github_branch_protection" "main" {
  repository_id = github_repository.repo.node_id
  pattern       = "main"

  required_status_checks {
    strict = true
    contexts = [
      "Test Summary",
      "firmware-tests",
      "rust-tests",
      "javascript-tests"
    ]
  }

  required_pull_request_reviews {
    dismiss_stale_reviews           = true
    require_code_owner_reviews      = false
    required_approving_review_count = 1
  }

  enforce_admins                  = true
  require_conversation_resolution = true
  require_signed_commits          = false
  allow_force_pushes             = false
  allow_deletions                = false
}
```

## Workflow Behavior

### On Push to Main
- All test jobs run automatically
- Results are visible in commit status
- Coverage reports uploaded to Codecov
- No blocking (already merged)

### On Pull Request
- All test jobs run on PR creation and each push
- **Test Summary job blocks merge** if any test fails
- Mutation testing runs but doesn't block (artifacts for review)
- Coverage diff shown in PR comments (via Codecov)
- Tests must pass and be up-to-date before merge button enables

### On Manual Trigger
- Can be triggered via GitHub Actions UI
- Useful for re-running tests without new commits

## Bypassing Checks (Emergency Only)

**Not recommended**, but if needed:
1. Administrators can bypass if "Enforce admins" is not checked
2. Use "Merge without waiting for requirements" in emergency
3. Document reason in PR comments
4. Fix failing tests in follow-up PR immediately

## Monitoring

### Check Status
```bash
# View recent workflow runs
gh run list --workflow=test.yml --limit 10

# View specific run details
gh run view <run-id>

# Watch live run
gh run watch
```

### Coverage Trends
- View at: https://codecov.io/gh/skyne/rp2040-ffb
- Set up Codecov comments on PRs for diff coverage

## Troubleshooting

### Tests Pass Locally But Fail in CI
- Check for environment differences (paths, dependencies)
- Verify all dependencies are in CI configuration
- Check for flaky tests (timing, race conditions)
- Review CI logs for specific errors

### Required Check Not Showing Up
- Check workflow file is on target branch
- Verify workflow has run at least once
- Status check name must match exactly (case-sensitive)
- Wait a few minutes for GitHub to recognize the check

### Cannot Merge Even Though Tests Pass
- Ensure branch is up-to-date with base branch
- Check all required checks have completed
- Verify no required conversations remain unresolved
- Check if required reviews are satisfied

## References

- [GitHub Branch Protection Documentation](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
- [Required Status Checks](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches#require-status-checks-before-merging)
- [GitHub Actions Status Checks](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/collaborating-on-repositories-with-code-quality-features/about-status-checks)
