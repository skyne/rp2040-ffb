# GitHub Scripts

Automation scripts for repository management.

## Setup Branch Protection

`setup-branch-protection.sh` - Configures branch protection rules for the `main` branch.

### Prerequisites

- [GitHub CLI (`gh`)](https://cli.github.com/) installed
- Authenticated with GitHub: `gh auth login`
- Admin permissions on the repository

### Usage

```bash
# From repository root
./.github/scripts/setup-branch-protection.sh

# Or from scripts directory
cd .github/scripts
./setup-branch-protection.sh
```

### What It Does

Configures the `main` branch with:

1. **Required Status Checks**
   - Test Summary
   - firmware-tests
   - rust-tests
   - javascript-tests
   - Must be up-to-date before merging

2. **Pull Request Requirements**
   - 1 required approving review
   - Dismiss stale reviews on new commits
   - Require conversation resolution

3. **Protections**
   - Block force pushes
   - Block branch deletion
   - Admins can bypass (for emergencies)

### Verification

After running, verify at:
```
https://github.com/OWNER/REPO/settings/branches
```

### Troubleshooting

**Error: GitHub CLI not installed**
```bash
# macOS
brew install gh

# Linux
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt update
sudo apt install gh

# Windows
winget install --id GitHub.cli
```

**Error: Not authenticated**
```bash
gh auth login
# Follow the prompts to authenticate
```

**Error: Insufficient permissions**
- You need admin access to the repository
- Ask a repository admin to run the script
- Or configure manually via GitHub UI

### Manual Configuration

If you prefer to configure via GitHub UI:

1. Go to: **Settings** → **Branches**
2. Click **Add branch protection rule**
3. Branch name pattern: `main`
4. Enable:
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
     - Select: `Test Summary`, `firmware-tests`, `rust-tests`, `javascript-tests`
   - ☑️ Require branches to be up to date before merging
   - ☑️ Require conversation resolution before merging
5. Click **Create** or **Save changes**

See `.github/BRANCH_PROTECTION.md` for detailed instructions.
