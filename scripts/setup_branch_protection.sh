#!/usr/bin/env bash
# Configure branch protection for main.
#
# GitHub requires GitHub Pro (or a public repository) for branch protection and
# rulesets on private repositories. If the API returns HTTP 403, enable Pro or
# make the repository public, then rerun this script.

set -euo pipefail

repository="${1:-chl84/3D_game_engine}"

gh api \
  --method PUT \
  -H "Accept: application/vnd.github+json" \
  "repos/${repository}/branches/main/protection" \
  --input - <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Linux (ci-linux-gcc14)",
      "Linux (ci-linux-clang)",
      "Windows (MSVC)",
      "Quality",
      "Linux ASan/UBSan"
    ]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null,
  "required_linear_history": false,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "required_conversation_resolution": true
}
JSON
