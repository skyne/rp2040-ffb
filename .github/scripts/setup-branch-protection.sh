#!/bin/bash
# Setup branch protection for main branch
# Requires: GitHub CLI (gh) to be installed and authenticated

set -e

REPO_OWNER="${GITHUB_REPOSITORY_OWNER:-skyne}"
REPO_NAME="${GITHUB_REPOSITORY_NAME:-rp2040-ffb}"
BRANCH="main"

echo "🔒 Setting up branch protection for ${REPO_OWNER}/${REPO_NAME}:${BRANCH}"
echo ""

# Check if gh is installed
if ! command -v gh &> /dev/null; then
    echo "❌ Error: GitHub CLI (gh) is not installed"
    echo "Install from: https://cli.github.com/"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo "❌ Error: Not authenticated with GitHub CLI"
    echo "Run: gh auth login"
    exit 1
fi

echo "✅ GitHub CLI authenticated"
echo ""

# Check if branch protection already exists
echo "📋 Checking existing branch protection..."
if gh api "repos/${REPO_OWNER}/${REPO_NAME}/branches/${BRANCH}/protection" &> /dev/null; then
    echo "⚠️  Branch protection already exists"
    read -p "Do you want to update it? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 0
    fi
fi

echo ""
echo "🔧 Applying branch protection rules..."
echo ""

# Apply branch protection
gh api \
  --method PUT \
  "repos/${REPO_OWNER}/${REPO_NAME}/branches/${BRANCH}/protection" \
  --field required_status_checks[strict]=true \
  --field required_status_checks[contexts][]=Test\ Summary \
  --field required_status_checks[contexts][]=firmware-tests \
  --field required_status_checks[contexts][]=rust-tests \
  --field required_status_checks[contexts][]=javascript-tests \
  --field required_pull_request_reviews[dismiss_stale_reviews]=true \
  --field required_pull_request_reviews[require_code_owner_reviews]=false \
  --field required_pull_request_reviews[required_approving_review_count]=1 \
  --field enforce_admins=false \
  --field required_conversation_resolution=true \
  --field allow_force_pushes=false \
  --field allow_deletions=false \
  --silent

echo "✅ Branch protection configured successfully!"
echo ""
echo "📝 Summary of protection rules:"
echo "   • Branch: ${BRANCH}"
echo "   • Required status checks:"
echo "     - Test Summary ✓"
echo "     - firmware-tests ✓"
echo "     - rust-tests ✓"
echo "     - javascript-tests ✓"
echo "   • Required PR reviews: 1"
echo "   • Dismiss stale reviews: Yes"
echo "   • Require conversation resolution: Yes"
echo "   • Enforce for admins: No (can bypass in emergency)"
echo ""
echo "🔗 View settings at:"
echo "   https://github.com/${REPO_OWNER}/${REPO_NAME}/settings/branches"
echo ""
echo "✨ Done! PRs to ${BRANCH} will now require all tests to pass."
